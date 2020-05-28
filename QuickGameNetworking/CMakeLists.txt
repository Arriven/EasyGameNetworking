add_library(QuickGameNetworking NetAPI.cpp NetData.cpp NetMessagesBase.cpp NetObject.cpp NetSocket.cpp)
target_compile_features(QuickGameNetworking PRIVATE cxx_std_17)
