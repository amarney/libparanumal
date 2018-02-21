PROJ_DIR:=$(dir $(abspath $(lastword $(MAKEFILE_LIST))))
ifndef OCCA_DIR
  include $(PROJ_DIR)/../../../scripts/makefile
else
  include ${OCCA_DIR}/scripts/makefile
endif

#---[ COMPILATION ]-------------------------------
headers = $(wildcard $(iPath)/*.hpp) $(wildcard $(iPath)/*.tpp)
sources = $(wildcard $(sPath)/*.cpp)

objects = $(subst $(sPath)/,$(oPath)/,$(sources:.cpp=.o))

# turn off HOST optimization
${PROJ_DIR}/BP30: $(objects) $(headers) ${PROJ_DIR}/kernelTestDriver.cpp
	$(compiler) $(compilerFlags) -o ${PROJ_DIR}/kernelTest $(flags) $(objects) ${PROJ_DIR}/kernelTestDriver.cpp $(paths) $(links)


$(oPath)/%.o:$(sPath)/%.cpp $(wildcard $(subst $(sPath)/,$(iPath)/,$(<:.cpp=.hpp))) $(wildcard $(subst $(sPath)/,$(iPath)/,$(<:.cpp=.tpp)))
	$(compiler) $(compilerFlags) -o $@ $(flags) -c $(paths) $<

clean:
	rm -f $(oPath)/*;
	rm -f ${PROJ_DIR}/BP3
#=================================================