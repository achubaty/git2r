% Generated by roxygen2: do not edit by hand
% Please edit documentation in R/index.r
\docType{methods}
\name{rm_file}
\alias{rm_file}
\alias{rm_file,git_repository,character-method}
\title{Remove files from the working tree and from the index}
\usage{
rm_file(repo, path)

\S4method{rm_file}{git_repository,character}(repo, path)
}
\arguments{
\item{repo}{The repository \code{object}.}

\item{path}{character vector with filenames to remove. The path
must be relative to the repository's working folder. Only files
known to Git are removed.}
}
\value{
invisible(NULL)
}
\description{
Remove files from the working tree and from the index
}
\examples{
\dontrun{
## Initialize a repository
path <- tempfile(pattern="git2r-")
dir.create(path)
repo <- init(path)

## Create a user
config(repo, user.name="Alice", user.email="alice@example.org")

## Create a file
writeLines("Hello world!", file.path(path, "file-to-remove.txt"))

## Add file to repository
add(repo, "file-to-remove.txt")
commit(repo, "First commit message")

## Remove file
rm_file(repo, "file-to-remove.txt")

## View status of repository
status(repo)
}

}
\keyword{methods}

