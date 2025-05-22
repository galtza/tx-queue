project "inter-consumer"
    kind "ConsoleApp"
    files {
        "utests/inter/consumer.cpp", 
        "utests/common/*.h", 
        "utests/common/*.inl", 
        "include/*.h", 
        "include/*.inl" 
    }

    debugargs { "-t" }

project "inter-producer"                                          
    kind "ConsoleApp"                                        
    files { 
        "utests/inter/producer.cpp",
        "utests/common/*.h",
        "utests/common/*.inl",
        "include/*.h",
        "include/*.inl" 
    }

    debugargs { "-t" }
