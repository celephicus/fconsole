make clean && \
make GCOV_CFLAGS="--coverage -O0" GCOV_LDFLAGS="-lgcov --coverage" && \
gcovr -f ../src/console.c -f main.c --html --html-details -o coverage/coverage.html
