KERNELVERSION=4.9.191
MOD_DESTDIR=/lib/modules/4.9.191/extra
check_module_directory /lib/modules/4.9.191
install_file dc_sunxi.ko ${MOD_DESTDIR}/dc_sunxi.ko "kernel_module" 0644 0:0
install_file pvrsrvkm.ko ${MOD_DESTDIR}/pvrsrvkm.ko "kernel_module" 0644 0:0
