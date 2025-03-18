project "inter-consumer"
    kind "ConsoleApp"
    files {
        "utests/inter/consumer.cpp", 
        "utests/common/*.h", 
        "utests/common/*.inl", 
        "utests/common/*.cpp", 
        "include/*.h", 
        "include/*.inl" 
    }

    filter { "configurations:debug"   } debugargs { "-t" }
    filter { "configurations:release" } debugargs { "-t" }
    filter {  }

project "inter-producer"                                          
    kind "ConsoleApp"                                        
    files { 
        "utests/inter/producer.cpp",
        "utests/common/*.h",
        "utests/common/*.inl",
        "utests/common/*.cpp",
        "include/*.h",
        "include/*.inl" 
    }

    filter { "configurations:debug"   } debugargs { "-t" }
    filter { "configurations:release" } debugargs { "-t" }
    filter {  }
