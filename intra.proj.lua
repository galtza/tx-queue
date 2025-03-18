project "intra"
    kind      "ConsoleApp"
    files     { "utests/intra/*", "utests/common/*.h", "utests/common/*.cpp", "include/*.h", "include/*.inl" }

    filter { "configurations:debug"   } debugargs { "-t" }
    filter { "configurations:release" } debugargs { "-t" }
    filter {  }
