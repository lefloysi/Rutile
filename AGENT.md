# Agent instructions

Before starting any task, read the project's AGENT.md file in full and follow its instructions
throughout the task. Do not rely on a remembered version. If the file changes, read it again before
continuing. If you cannot find or read it, say so explicitly rather than assuming its contents.

## Read before writing

Read the relevant source and comparable implementations before making changes. Understand the
existing conventions and use them consistently. Search for existing types and helpers before
introducing replacements.

Treat examples as evidence of a convention, not as rules tied to a particular name, type, function,
or file. Where conventions vary, follow the surrounding code and the relevant API family.

## Responding to corrections

- When the user points out a problem or states a correction, just do it. Treat it as an instruction
  to make the change immediately, without waiting for a separate request to edit.
- Do not substitute agreement, paraphrasing, or an offer to fix it for actually making the change.
- Check for the same issue in related code and apply the correction consistently.
- Verify the change and report what was fixed. If blocked, explain the concrete blocker.
- Learn the general principle behind feedback. Document that principle rather than a rule tied
  to the particular command, name, or implementation that prompted it.
- Use brief prose examples to illustrate the principle, without making them exhaustive rules.
  For example, feedback about an uninformative status message means commands should report useful
  information about the user's work, rather than merely confirming an internal process exists.

## Error handling

Expected failures such as missing files, denied access, invalid input, and rejected configuration
must be returned as explicit results or status codes. Inspect and propagate those results at call
sites; do not convert them into exceptions or use try/catch as ordinary control flow. Reserve
exceptions for compiler failures.

## Language design decisions

- Ask the user before introducing language features or semantics that have not been specified.
- Do not infer the new language's design from the implementation language or familiar languages.
- Distinguish implementation choices from decisions about what programs can express and mean.
  For example, implementing a parser does not authorize adding pointer support to the language.
- Continue work on specified behavior while awaiting decisions about unspecified features.

## Command-line behavior

- Require an explicit operation. Treat a missing operation as an error and direct the user to help.
- Make command output serve the user's purpose. Status commands should describe meaningful state
  and relevant statistics, rather than only reporting whether an internal service is running.
- Choose status information from actual project behavior and user requirements. Do not invent
  statistics or expose implementation details merely to fill the output.
- For example, invoking a tool without an operation should point to its help command. Requesting
  status should explain the state of the user's work with available facts and useful measurements.

## Goals and subagents

Always use goals and subagents when performing work.

- Create a goal for the requested outcome before starting work, or continue the existing goal when
  it already covers the request. Keep it active until the full outcome is verified.
- Use subagents to investigate, implement, or review clearly scoped parts of the work. Give each a
  concrete task and enough context to complete it.
- Coordinate shared work so agents do not overwrite each other's changes. Combine their findings
  and verify the result; delegation does not transfer responsibility for correctness.
- Continue through intermediate results until the requested goal is complete. Do not treat a plan,
  partial implementation, or subagent report as proof of completion.

## Versioning and commits

- Evolve the compiler through staged self-hosting. Add support using syntax the current Rutile
  compiler understands, compile the updated compiler, then use the new features in its source.
- Use the distributed Rutile compiler as the bootstrap seed and keep it matched to its source.

- Commit completed, verified changes as work progresses.
- Keep the release version in the root VERSION file as the single source of truth.
- Increment the patch component for each completed change batch: 0.0.2, 0.0.3, and onward.
- Keep the major and minor components at zero until the user explicitly authorizes 0.1.0.
- Keep the communication protocol version separate; change it only for protocol changes.

## Source formatting

- Indent with tabs displayed at four columns.
- Attach opening braces to declarations and control statements.
- Always use braces for control-statement bodies, including single statements. Never omit them.
- Handle errors and exceptional cases first with early exits. Keep the happy path last.
- Let the happy path proceed directly instead of nesting it inside an else branch.
- Use while (true) for infinite loops.
- Exit such loops with break when the stopping condition is reached.
  Perform required cleanup first.
- Do not introduce a boolean flag solely to control loop termination.
- Use logical negation when checking for zero instead of an explicit equality comparison.
- Place an else clause on a new line after the preceding closing brace.
- Put a space before control-statement parentheses. Do not put a space before function-call or
  function-declaration parentheses.
- Put spaces around binary and assignment operators and after commas. Keep member access and scope
  resolution tight.
- Attach pointer and reference symbols to the type.
- Do not put a space between the template keyword and its opening angle bracket.
- Put spaces inside nonempty braced initializer lists. Keep empty initializer braces adjacent.
- Do not impose a fixed column limit. Keep straightforward declarations and expressions on one
  line; wrap when it improves readability.
- Keep argument and parameter lists on one line.
- When a parenthesized expression spans multiple lines, give every continuation term its own line.
  Put continuation operators at the start of those lines. Put the closing parenthesis on a separate
  line before the attached opening brace, never at the end of the final term.
- Keep multiline conditions consistent from the first line, rather than starting with an operand
  inline and wrapping only the remaining operands.
- Separate functions and distinct declaration groups with one blank line.
- Separate validation, preparation, mutation, and result handling with logical blank lines.
- Write function bodies on multiple lines, even when they contain only one statement.
- Write type definitions on multiple lines, with each member or enumerator on its own line.
- Indent namespace contents consistently with surrounding code.
- Preserve a consistent comment style. Namespace closing comments identify the namespace.

## Naming

- Use snake_case for ordinary variables, parameters, data members, and utility functions.
- Use PascalCase for type names, except simple types, which use snake_case names.
- Follow the naming convention of the relevant type or API family. Do not force one capitalization
  scheme onto families with different established conventions.
- Use uppercase names with underscores for macros.
- Use snake_case names for enums and their scoped members. Do not repeat the enum's name in its
  members. Use role prefixes where they distinguish categories, such as keywords and operators.
- Express nested namespaces with qualified declarations where appropriate.

## Data representation

- Represent meaningful alternatives structurally rather than accumulating boolean flags.
- Prefer a representation that expresses valid states directly and prevents contradictory states.
- Treat booleans as an exception requiring justification, rather than the default representation.
- Consider storage size, padding, and redundant information when designing data structures.
- For example, distinct numeric categories should have an explicit representation rather than
  independent flags whose combinations may be meaningless.

## Declarations and initialization

- Do not use internal linkage.
- Do not typedef structs. Use the struct keyword with the type name directly.
- Keep all implementation in source files. Do not put implementation in header files.
- Include platform-specific headers only in source files designed to be replaced for each platform.
- Use pragma-once header protection for C++ headers.
- Include the corresponding header first in implementation files. Group related project headers and
  standard headers separately.
- Use the project's established numeric aliases when available.
- Use explicit types where they communicate intent. Use inferred types where the type is apparent
  or inference is natural to generic code.
- Choose initialization syntax that expresses intent clearly and avoids ambiguous declarations.
  Preserve consistent surrounding usage.
- Respect the language's move and copy semantics. Copy when the source must remain valid;
  move when its value is consumed, including a parameter's final use.
- Pass primitives and small non-owning views by value for ordinary input parameters. Keep references
  when receiver identity, copy construction, or mutation of the caller's object requires them.
- Apply declaration qualifiers when they express an actual contract.
- Use named casts in C++. Follow the language's own conversion syntax in C source.
- End Rutile enum entries with a semicolon. Use trailing commas in C and C++ enums.

## File organization

- Keep the repository root for files that need a root-level location, such as build and repository
  configuration. Put generated logs, temporary scripts, binaries, and intermediate files under
  build/ or an appropriate component directory. Run tools from a suitable output directory or
  specify all output paths explicitly so they do not leave artifacts in the root.
- Use the .hpp extension for headers in both C and C++, rather than .h.
- When multiple files share a naming prefix, consider grouping them in a folder for that concept.
- Remove the shared prefix from filenames inside that folder.

## CMake formatting

- Always enclose text in double quotes in CMake files.
- Declare targets first, then add their sources separately with target_sources.

## Markdown formatting

- Keep every line in Markdown files under 100 characters. Insert newlines to wrap long lines.
- Use plain ASCII characters instead of typographic symbols. For example, use ->.

## Applying conventions

Keep this guidance general and self-contained. Use brief prose examples to explain principles.
Do not encode individual implementations, external source paths, or copied code as project rules.
Resolve unanswered questions by inspecting comparable code in the current project.

