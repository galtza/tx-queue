-- Copyright © 2017-2025 Raúl Ramos García. All rights reserved.

project "intra"
    kind      "ConsoleApp"
    files     { "utests/intra/*", "utests/common/*.h", "utests/common/*.cpp", "include/*.h", "include/*.inl" }

    debugargs { "-t" }
