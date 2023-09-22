# Patches for SeL4 core code
This directory contains patches that apply to SeL4 kernel and core libraries.
Patches must be applied in order for the project to function properly. The path
here in which the patches reside is the same path that one must move them to in
order to apply the patch. e.g. to apply the projects/seL4_libs/private.h.patch
one must do the following:
```bash
cd projects/seL4_libs
cp ../../projects/seL4_libs/private.h.patch
```

Current patches are described below:
## projects/seL4_libs
Patches to be added for the `seL4_libs` library set.

- private.h.patch, sel4bench.h.armv8.patch, sel4bench.h.gen.patch: Removes
  static inline definition from functions that read and write the programme
  counters in order to be visible as functions by the pass during injection.