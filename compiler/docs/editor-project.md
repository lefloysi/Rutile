# Editor source groups

Kiln analyzes each configured Rutile target with its source folders and the Rutile standard
library. Files outside those targets belong to a loose group of open documents.

This project's `kiln.rt` declares `compiler/src` as the compiler's source folder. The sibling
Kiln project's configuration includes both `../Rutile/compiler/src` and `../Rutile/language`
in its compiler/service object target. Sources are discovered recursively and sorted by path.
Configuration programs are excluded from the target's compiler input.

The Rutile compiler collects declaration owners before resolving qualified declarations, so
source discovery does not require an ordered source manifest.
