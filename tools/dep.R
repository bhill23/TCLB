#!/usr/bin/env Rscript

library(optparse)
options <- list(
        make_option(c("-d","--path"), "store", default="", help="Work directory", type="character")
)
opt <- parse_args(OptionParser(usage="Usage: ADmod -f inputfile [-o outputfile]", options))

if (opt$path != "") setwd(opt$path)

# 'grep' all the includes
f = pipe("grep '# *include' `find . -type f \\( -name '*.c' -o -name '*.cu' -o -name '*.cpp' -o -name '*.h' -o -name '*.hpp' \\)` | sed -n -e 's/^\\([^:]*\\):[ \\t]*#include[ \\t]*[\"<]\\(.*\\)[>\"]/\\1,\\2/gp'")
w = read.csv(f,col.names=c("file","dep"), stringsAsFactor=F, header=FALSE);

# make the relative paths in include statement relative to *here*
w[,2] = paste0(sub("[^/]*$","",w[,1]),w[,2])

# cut out system includes
sel = sapply(w[,2],file.exists)
w = w[sel,]

# function reducing the . and ..
resolve.path = function(x) sapply(strsplit(x,"/"),function(x) {
  x = x[x!="."];
  while (any(x[-1] == ".." & x[-length(x)] != "..")) {
    i = which(x[-1] == ".." & x[-length(x)] != "..")
    x = x[-c(i,i+1)]
  }
  paste(x,collapse="/")
})

# reduce the paths in w
w[,1] = resolve.path(w[,1])
w[,2] = resolve.path(w[,2])

# create the makefile-style dependency lines
dep = do.call(c,as.list(by(w,w$file,function(x) paste(x[1,1],paste(x[,2],collapse=" "),sep=" : ") )))

# write dep.mk
f=file("dep.mk")
if (length(dep) > 0) {
	cat(paste(dep,"\n#\t@echo \"  DEP        $@ <-- $?\"\n\t@test -f $@ && touch $@\n\n",sep=""),sep="", file=f)
} else {
	cat("# no dep\n",file=f);
}
