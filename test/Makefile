# Automatically clean and rebuild on makefile changes https://stackoverflow.com/a/5901262/871842
-include .makefile_date
.makefile_date : Makefile*
	@touch $@
	+$(MAKE) --no-print-directory clean

ifndef BUILD_CONFIGS
BUILD_CONFIGS=clang++-10 clang++-9 g++-9 clang++-8 g++-8 clang++-7
endif

# https://stackoverflow.com/a/3618308/871842
# export FIX_PATH_STDOUT = | sed -r 's/^.*:[[:digit:]]+:[[:digit:]]+\:/$(notdir $(shell pwd))\/\0/g'
# export FIX_PATH_STDERR = 3>&1 1>&2 2>&3 3>&- $(FIX_PATH_STDOUT)

CATCH2_FLAGS = --use-colour yes # --benchmark-samples 1000

TARGET_EXES=$(addsuffix _test.exe, $(BUILD_CONFIGS))

runtests : $(addsuffix _test_output.txt, $(BUILD_CONFIGS))
binaries : $(TARGET_EXES)

%_valgrind : %_test.exe
	valgrind ./$<

%_test_output.txt : %_test.exe
	stdbuf -o0 ./$< $(CATCH2_FLAGS) | tee $@

$(TARGET_EXES) : %_test.exe :
	+$(MAKE) -f Makefile2 --no-print-directory CXX=$*

clean :
	rm -rf out *.exe *_test_output.txt

.PHONY : $(TARGET_EXES) clean
.PRECIOUS: %_test.exe
