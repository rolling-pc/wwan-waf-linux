common:抽象层公共代码，不区分平台，如接口类定义，初始化
mtk/pcie:所有基于pcie相关的功能实现代码
mtk/pcie/linux/binary_common:厂商提供的通用PCIE二进制程序或代码
mtk/pcie/linux/pcie_common.cc:linux通用pcie接口实现代码
mtk/pcie/linux/rw350:产品差异化接口实现
mtk/pcie/linux/rw350/binary:芯片平台提供产品特有的差异化的二进制程序或代码，如不能和binary_common兼容的

