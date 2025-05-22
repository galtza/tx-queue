project "intra"
    kind "ConsoleApp"
    files { 
        "utests/intra/*", 
        "utests/common/*.h", 
        "utests/common/*.inl", 
        "include/*.h", 
        "include/*.inl" 
    }

    debugargs { "-t" }
