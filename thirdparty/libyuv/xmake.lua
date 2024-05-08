package("libyuv")

    set_homepage("https://chromium.googlesource.com/libyuv/libyuv/")
    set_description("libyuv is an open source project that includes YUV scaling and conversion functionality.")
    set_license("BSD-3-Clause")
    -- add_versions("20210528", "eb6e7bb63738e29efd82ea3cf2a115238a89fa51")

    -- set_urls("https://chromium.googlesource.com/libyuv/libyuv.git")
    -- add_versions("2023.10.27", "31e1d6f896615342d5d5b6bde8f7b50b3fd698dc")

    set_sourcedir(os.scriptdir())
    add_deps("cmake")

    on_install("windows", "linux", "macosx", "android", "cross", "bsd", "mingw", function (package)
        local configs = {"-DTEST=OFF"}
        table.insert(configs, "-DCMAKE_BUILD_TYPE=" .. (package:debug() and "Debug" or "Release"))
        
        io.replace("CMakeLists.txt", "INSTALL ( PROGRAMS ${CMAKE_BINARY_DIR}/yuvconvert			DESTINATION bin )", "", {plain = true})
        import("package.tools.cmake").install(package, configs)
        
        if package:is_plat("macosx", "linux", "android") then
            if package:config("shared") then 
                os.tryrm(package:installdir("lib", "*.a"))
            else 
                os.tryrm(package:installdir("lib", "*.so"))
            end
        end
    end)
