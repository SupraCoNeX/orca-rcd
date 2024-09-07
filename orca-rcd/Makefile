#
# Copyright (C) 2021 Felix Fietkau <nbd@nbd.name>
# Copyright (C) 2021-2024 SupraCoNeX <supraconex@gmail.com>
#
# This is free software, licensed under the GNU General Public License v2.
# See /LICENSE for more information.
#

include $(TOPDIR)/rules.mk

PKG_NAME:=orca-rcd
PKG_VERSION:=3.0.0
PKG_RELEASE:=1

PKG_BUILD_DIR := $(BUILD_DIR)/$(PKG_NAME)-$(PKG_VERSION)-$(PKG_RELEASE)

PKG_CONFIG_DEPENDS:= \
  CONFIG_MQTT_SUPPORT \
  CONFIG_ZSTD_COMPRESSION

include $(INCLUDE_DIR)/package.mk
include $(INCLUDE_DIR)/cmake.mk

define Package/orca-rcd
  SECTION:=utils
  CATEGORY:=Utilities
  TITLE:=Remote control daemon for ORCA
  DEPENDS:=+libubox +libuci +MQTT_SUPPORT:libmosquitto-ssl +ZSTD_COMPRESSION:libzstd
  MAINTAINER:=Felix Fietkau <nbd@nbd.name>
endef

define Package/orca-rcd/description
  Daemon for remote controlling ORCA (Open-Source Resource Control API).
endef

define Package/orca-rcd/conffiles
/etc/config/orca-rcd
endef

define Package/orca-rcd/config
	config MQTT_SUPPORT
	  bool "mqtt support"
	  default n

	config ZSTD_COMPRESSION
	  bool "zstd compression"
	  default n
endef

ifeq ($(CONFIG_MQTT_SUPPORT),y)
	CMAKE_OPTIONS += -DCMAKE_CONFIG_MQTT=y
else
	CONFFILE:=orca-rcd.config
endif

ifeq ($(CONFIG_ZSTD_COMPRESSION),y)
	CMAKE_OPTIONS += -DCMAKE_CONFIG_ZSTD=y
	DICTFILE:=dictionary.zdict
endif

define Package/orca-rcd/install
	$(INSTALL_DIR) $(1)/usr/sbin $(1)/etc/init.d $(1)/etc/config
	$(INSTALL_BIN) ./files/orca-rcd.init $(1)/etc/init.d/orca-rcd
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/orca-rcd $(1)/usr/sbin/
	$(INSTALL_DATA) ./files/orca-rcd.config $(1)/etc/config/orca-rcd
ifeq ($(CONFIG_ZSTD_COMPRESSION),y)
	$(INSTALL_DIR) $(1)/lib/orca-rcd
	$(INSTALL_DATA) ./files/$(DICTFILE) $(1)/lib/orca-rcd/dictionary.zdict
endif
endef

$(eval $(call BuildPackage,orca-rcd))

