#include "Emit/SpirvEmitter.hpp"

#include <spirv/unified1/spirv.h>

#include <algorithm>
#include <span>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace rt {
namespace {

struct SpvWriter {
    std::vector<u32> words;
    u32 bound = 1;
};

static void begin(SpvWriter &w) {
    w.words = {SpvMagicNumber, SpvVersion, 0, 0, 0};
}

static void emit(SpvWriter &w, SpvOp op, const std::vector<u32> &ops) {
    w.words.push_back(((1u + static_cast<u32>(ops.size())) << SpvWordCountShift) | static_cast<u32>(op));
    w.words.insert(w.words.end(), ops.begin(), ops.end());
}

static void emit(SpvWriter &w, SpvOp op, std::initializer_list<u32> ops) {
    emit(w, op, std::vector<u32>(ops));
}

static void reserve(SpvWriter &w, u32 id) {
    w.bound = std::max(w.bound, id + 1);
}

static u32 fresh(SpvWriter &w) {
    return w.bound++;
}

static const RTFunction *find_stage_function(const RTArtifactModule &module, RTStageKind stage) {
    for (const auto &fn : module.functions) {
        if (fn.stage == stage) {
            return &fn;
        }
    }
    return nullptr;
}

static bool has_type_id(const RTArtifactModule &module, u32 type_id) {
    if (!type_id) return false;
    for (const auto &inst : module.type_constant_pool) {
        if (inst.result_id == type_id) return true;
    }
    return false;
}

static bool validate_type_refs(const RTArtifactModule &module, std::string *err) {
    for (const auto &inst : module.type_constant_pool) {
        for (u32 operand : inst.operands) {
            if (!has_type_id(module, operand)) {
                if (err) *err = "dangling RTSLP type reference " + std::to_string(operand);
                return false;
            }
        }
    }
    return true;
}

static bool is_type_op(RTIROp op) {
    return op >= RTIROp::TypeVoid && op <= RTIROp::TypeSampledImage;
}

static std::vector<u32> make_words(std::initializer_list<u32> ops) {
    return std::vector<u32>(ops.begin(), ops.end());
}

static bool emit_stage(const RTArtifactModule &module, RTStageKind stage, std::vector<u32> *out) {
    const RTFunction *fn = find_stage_function(module, stage);
    if (!fn) return false;

    std::string err;
    if (!validate_type_refs(module, &err)) {
        (void)err;
        return false;
    }

    SpvWriter w;
    begin(w);
    std::vector<u32> type_ops;
    std::vector<u32> fn_ops;

    emit(w, SpvOpCapability, {SpvCapabilityShader});
    emit(w, SpvOpMemoryModel, {SpvAddressingModelLogical, SpvMemoryModelGLSL450});

    for (const auto &inst : module.type_constant_pool) {
        if (inst.result_id) reserve(w, inst.result_id);
    }
    for (const auto &func : module.functions) {
        if (func.result_id) reserve(w, func.result_id);
        for (const auto &inst : func.body) {
            if (inst.result_id) reserve(w, inst.result_id);
        }
    }

    auto type_emit = [&](SpvOp op, std::initializer_list<u32> ops) {
        type_ops.push_back(((1u + static_cast<u32>(ops.size())) << SpvWordCountShift) | static_cast<u32>(op));
        type_ops.insert(type_ops.end(), ops.begin(), ops.end());
    };
    auto type_emit_v = [&](SpvOp op, const std::vector<u32> &ops) {
        type_ops.push_back(((1u + static_cast<u32>(ops.size())) << SpvWordCountShift) | static_cast<u32>(op));
        type_ops.insert(type_ops.end(), ops.begin(), ops.end());
    };

    // Dedup non-aggregate types: SPIR-V forbids duplicate TypeVoid/TypeBool/
    // TypeInt/TypeFloat/TypeVector/TypeMatrix/TypePointer/TypeFunction.
    // The RTSLP type pool can contain logically-identical entries with distinct
    // result_ids (e.g. several `float`s), so we collapse them by canonical
    // signature and remap downstream references to the first id seen.
    std::unordered_map<u32, u32> id_remap;
    auto canon = [&](u32 id) -> u32 {
        auto it = id_remap.find(id);
        return it == id_remap.end() ? id : it->second;
    };

    struct TypeKey {
        u32 op;
        u32 a, b, c;
        bool operator==(const TypeKey &o) const { return op == o.op && a == o.a && b == o.b && c == o.c; }
    };
    struct TypeKeyHash {
        size_t operator()(const TypeKey &k) const {
            return std::hash<u64>{}(((u64)k.op << 32) ^ k.a) ^ (std::hash<u32>{}(k.b) << 1) ^ (std::hash<u32>{}(k.c) << 2);
        }
    };
    std::unordered_map<TypeKey, u32, TypeKeyHash> seen_type;

    auto try_dedup = [&](const auto &inst, u32 a, u32 b, u32 c) -> bool {
        TypeKey k{(u32)inst.op, a, b, c};
        auto it = seen_type.find(k);
        if (it != seen_type.end()) {
            id_remap[inst.result_id] = it->second;
            return true;
        }
        seen_type[k] = inst.result_id;
        return false;
    };

    for (const auto &inst : module.type_constant_pool) {
        if (!inst.result_id || !is_type_op(inst.op)) continue;
        switch (inst.op) {
        case RTIROp::TypeVoid:
            if (try_dedup(inst, 0, 0, 0)) break;
            type_emit(SpvOpTypeVoid, {inst.result_id});
            break;
        case RTIROp::TypeBool:
            if (try_dedup(inst, 0, 0, 0)) break;
            type_emit(SpvOpTypeBool, {inst.result_id});
            break;
        case RTIROp::TypeInt: {
            u32 width = inst.literals.empty() ? 32u : inst.literals[0];
            if (try_dedup(inst, width, 1, 0)) break;
            type_emit(SpvOpTypeInt, {inst.result_id, width, 1});
            break;
        }
        case RTIROp::TypeUInt: {
            u32 width = inst.literals.empty() ? 32u : inst.literals[0];
            if (try_dedup(inst, width, 0, 0)) break;
            type_emit(SpvOpTypeInt, {inst.result_id, width, 0});
            break;
        }
        case RTIROp::TypeFloat: {
            u32 width = inst.literals.empty() ? 32u : inst.literals[0];
            if (try_dedup(inst, width, 0, 0)) break;
            type_emit(SpvOpTypeFloat, {inst.result_id, width});
            break;
        }
        case RTIROp::TypeVector:
            if (!inst.operands.empty() && !inst.literals.empty()) {
                u32 component = canon(inst.operands[0]);
                u32 count = inst.literals[0];
                if (try_dedup(inst, component, count, 0)) break;
                type_emit(SpvOpTypeVector, {inst.result_id, component, count});
            }
            break;
        case RTIROp::TypeMatrix:
            if (!inst.operands.empty() && !inst.literals.empty()) {
                u32 column = canon(inst.operands[0]);
                u32 count = inst.literals[0];
                if (try_dedup(inst, column, count, 0)) break;
                type_emit(SpvOpTypeMatrix, {inst.result_id, column, count});
            }
            break;
        case RTIROp::TypeStruct: {
            // Structs are not deduplicated (each declaration is distinct).
            std::vector<u32> ops{inst.result_id};
            for (u32 o : inst.operands) ops.push_back(canon(o));
            type_emit_v(SpvOpTypeStruct, ops);
            break;
        }
        case RTIROp::TypePointer:
            if (!inst.operands.empty() && !inst.literals.empty()) {
                u32 storage = inst.literals[0];
                u32 pointee = canon(inst.operands[0]);
                if (try_dedup(inst, storage, pointee, 0)) break;
                type_emit(SpvOpTypePointer, {inst.result_id, storage, pointee});
            }
            break;
        default:
            break;
        }
    }

    u32 void_type = 0;
    for (const auto &inst : module.type_constant_pool) {
        if (inst.op == RTIROp::TypeVoid) {
            void_type = canon(inst.result_id);
            break;
        }
    }
    if (!void_type) return false;

    // Emit a function type "void()" for the entry point and an entry-point id.
    const u32 fn_type_id = fresh(w);
    type_emit(SpvOpTypeFunction, {fn_type_id, void_type});

    const u32 entry_id = fresh(w);
    const u32 label_id = fresh(w);

    // OpEntryPoint must appear before types/functions. Build it into the
    // header section (after capabilities/memory model, before types).
    // Name "main\0" packed little-endian: 'm'|'a'<<8|'i'<<16|'n'<<24 = 0x6e69616d
    emit(w, SpvOpEntryPoint, {
        static_cast<u32>(stage == RTStageKind::Vertex ? SpvExecutionModelVertex : SpvExecutionModelFragment),
        entry_id,
        0x6e69616d,
        0
    });
    if (stage == RTStageKind::Fragment) {
        emit(w, SpvOpExecutionMode, {entry_id, SpvExecutionModeOriginUpperLeft});
    }

    auto fn_emit = [&](SpvOp op, std::initializer_list<u32> ops) {
        fn_ops.push_back(((1u + static_cast<u32>(ops.size())) << SpvWordCountShift) | static_cast<u32>(op));
        fn_ops.insert(fn_ops.end(), ops.begin(), ops.end());
    };
    fn_emit(SpvOpFunction, {void_type, entry_id, SpvFunctionControlMaskNone, fn_type_id});
    fn_emit(SpvOpLabel, {label_id});
    fn_emit(SpvOpReturn, {});
    fn_emit(SpvOpFunctionEnd, {});

    w.words.insert(w.words.end(), type_ops.begin(), type_ops.end());
    w.words.insert(w.words.end(), fn_ops.begin(), fn_ops.end());
    w.words[3] = w.bound;
    *out = std::move(w.words);
    return true;
}

} // namespace

bool emit_rtslp_stage_spirv(const RTArtifactModule &module, RTStageKind stage, std::vector<u32> *spirv_out, std::string *error_out) {
    (void)error_out;
    return emit_stage(module, stage, spirv_out);
}

} // namespace rt
