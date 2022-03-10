cmake . -DRAUC_SYSTEM_CONF_PATH=/etc/rauc/system.conf \
		-DCMAKE_BUILD_TYPE=Release \
		-DNAND_RAUC_SYSTEM_CONF_PATH=/etc/rauc/system.conf.nand \
		-DEMMC_RAUC_SYSTEM_CONF_PATH=/etc/rauc/system.conf.mmc \
		-DUBOOT_ENV_PATH=/etc/fw_env.config \
		-DNAND_UBOOT_ENV_PATH=/etc/fw_env.config.nand \
		-DEMMC_UBOOT_ENV_PATH=/etc/fw_env.config.mmc
