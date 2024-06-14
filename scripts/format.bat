FOR /R "source" %%G IN (*.cpp *.hpp) DO (
    clang-format -i "%%G"
)
echo All formatted!