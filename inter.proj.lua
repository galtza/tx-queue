-- Copyright © 2017-2025 Raúl Ramos García. All rights reserved.

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

    debugargs { "-t" }

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

    debugargs { "-t" }
