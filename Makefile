include $(TOPDIR)/rules.mk
PKG_NAME:=AT_Tool_ApiServer
PKG_VERSION:=1.8.2
PKG_BUILD_DIR:= $(BUILD_DIR)/$(PKG_NAME)

include $(INCLUDE_DIR)/package.mk

define Package/AT_Tool_ApiServer
	SECTION:=wrtnode
	CATEGORY:=Daocaoren
	SUBMENU :=AT Tool
	TITLE:=AT_Tool_ApiServer AT
	DEPENDS:=+libpthread#
endef
 
define Package/AT_Tool_ApiServer/description
	A sample for AT_Tool_ApiServer
endef
 
define Build/Prepare
	mkdir -p $(PKG_BUILD_DIR)
	$(CP) ./src/* $(PKG_BUILD_DIR)/
endef
 
 
define Package/AT_Tool_ApiServer/install
	$(INSTALL_DIR) $(1)/bin
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/AT_Tool_ApiServer $(1)/bin/
endef
 
$(eval $(call BuildPackage,AT_Tool_ApiServer))
