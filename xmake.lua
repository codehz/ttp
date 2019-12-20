set_project("ttp")
set_version("0.0.1")
set_xmakever("2.2.8")
set_warnings("all", "error")
set_languages("gnu15")

add_rules("mode.debug", "mode.release")

if is_mode("debug") then
    set_symbols("debug")
    set_optimize("smallest")
    add_defines("__tb_debug__")
    if is_mode("check") and is_arch("i386", "x86_64") then
        add_cxflags("-fsanitize=address", "-ftrapv")
        add_mxflags("-fsanitize=address", "-ftrapv")
        add_ldflags("-fsanitize=address")
    end
end

if is_mode("release") then
    set_symbols("hidden")
    set_strip("all")
    add_cxflags("-fomit-frame-pointer")
    add_mxflags("-fomit-frame-pointer")
    set_optimize("fastest")
    add_vectorexts("sse2", "sse3", "ssse3", "mmx")
end

add_requires("tbox dev", {configs = {coroutine = true, debug = is_mode("debug")}})
add_includedirs("include")

target("ttp")
    set_kind("static")
    add_defines("__tb_prefix__=\"ttp\"")
    add_files("src/*.c") 
    add_headerfiles("include/**.h")
    add_packages("tbox")

option("demo")
    set_default(false)
    set_showmenu(true)
    set_category("option")
    set_description("Enable or disable the demo module")
option_end()

if has_config("demo") then
    target("demo_test")
        set_kind("binary")
        add_defines("__tb_prefix__=\"demo_test\"")
        add_files("demo/test.c")
        add_deps("ttp")
        add_packages("tbox", "ttp")

    target("demo_hello")
        set_kind("binary")
        add_defines("__tb_prefix__=\"demo_hello\"")
        add_files("demo/hello.c")
        add_deps("ttp")
        add_packages("tbox", "ttp")
end
