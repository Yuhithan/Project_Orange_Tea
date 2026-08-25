file(REMOVE_RECURSE
  "CMakeFiles/ortos_kernel"
  "kernel.bin"
  "ortos-objects/boot_header.o"
  "ortos-objects/boot_main.o"
  "ortos-objects/boot_main64.o"
  "ortos-objects/boot_mode.o"
  "ortos-objects/framebuffer.o"
  "ortos-objects/irq.o"
  "ortos-objects/keyboard.o"
  "ortos-objects/main.o"
  "ortos-objects/network.o"
  "ortos-objects/print.o"
  "ortos-objects/shell.o"
  "ortos-objects/storage.o"
  "ortos-objects/test.o"
  "ortos-objects/timer.o"
)

# Per-language clean rules from dependency scanning.
foreach(lang )
  include(CMakeFiles/ortos_kernel.dir/cmake_clean_${lang}.cmake OPTIONAL)
endforeach()
