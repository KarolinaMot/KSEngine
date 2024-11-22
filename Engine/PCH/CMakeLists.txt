add_library(PCH)
target_sources(PCH PUBLIC "pch.hpp" PRIVATE "pch.cpp")
target_precompile_headers(PCH PRIVATE "pch.hpp")
