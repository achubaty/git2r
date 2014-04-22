/*
 *  git2r, R bindings to the libgit2 library.
 *  Copyright (C) 2013-2014 The git2r contributors
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License, version 2,
 *  as published by the Free Software Foundation.
 *
 *  git2r is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include <Rdefines.h>

#include "git2r_commit.h"
#include "git2r_error.h"
#include "git2r_repository.h"
#include "git2r_signature.h"
#include "git2r_tree.h"

/**
 * Commit
 *
 * @param repo S4 class git_repository
 * @param message
 * @param author S4 class git_signature
 * @param committer S4 class git_signature
 * @param parent_list
 * @return S4 class git_commit
 */
SEXP commit(SEXP repo, SEXP message, SEXP author, SEXP committer, SEXP parent_list)
{
    SEXP when, sexp_commit;
    int err;
    int changes_in_index = 0;
    const char* err_msg = NULL;
    git_signature *sig_author = NULL;
    git_signature *sig_committer = NULL;
    git_index *index = NULL;
    git_oid commit_id, tree_oid;
    git_repository *repository = NULL;
    git_tree *tree = NULL;
    size_t i, count;
    git_commit **parents = NULL;
    git_commit *new_commit = NULL;
    git_status_list *status = NULL;
    git_status_options opts = GIT_STATUS_OPTIONS_INIT;
    opts.show  = GIT_STATUS_SHOW_INDEX_ONLY;

    if (check_string_arg(message)
        || check_signature_arg(author)
        || check_signature_arg(committer)
        || R_NilValue == parent_list
        || !isString(parent_list))
        error("Invalid arguments to commit");

    repository = get_repository(repo);
    if (!repository)
        error(git2r_err_invalid_repository);

    when = GET_SLOT(author, Rf_install("when"));
    err = git_signature_new(&sig_author,
                            CHAR(STRING_ELT(GET_SLOT(author, Rf_install("name")), 0)),
                            CHAR(STRING_ELT(GET_SLOT(author, Rf_install("email")), 0)),
                            REAL(GET_SLOT(when, Rf_install("time")))[0],
                            REAL(GET_SLOT(when, Rf_install("offset")))[0]);
    if (err < 0)
        goto cleanup;

    when = GET_SLOT(committer, Rf_install("when"));
    err = git_signature_new(&sig_committer,
                            CHAR(STRING_ELT(GET_SLOT(committer, Rf_install("name")), 0)),
                            CHAR(STRING_ELT(GET_SLOT(committer, Rf_install("email")), 0)),
                            REAL(GET_SLOT(when, Rf_install("time")))[0],
                            REAL(GET_SLOT(when, Rf_install("offset")))[0]);
    if (err < 0)
        goto cleanup;

    err = git_status_list_new(&status, repository, &opts);
    if (err < 0)
        goto cleanup;

    count = git_status_list_entrycount(status);
    for (i = 0; i < count; ++i) {
        const git_status_entry *s = git_status_byindex(status, i);

        if (s->status == GIT_STATUS_CURRENT)
            continue;

        if (s->status & GIT_STATUS_INDEX_NEW)
            changes_in_index = 1;
        else if (s->status & GIT_STATUS_INDEX_MODIFIED)
            changes_in_index = 1;
        else if (s->status & GIT_STATUS_INDEX_DELETED)
            changes_in_index = 1;
        else if (s->status & GIT_STATUS_INDEX_RENAMED)
            changes_in_index = 1;
        else if (s->status & GIT_STATUS_INDEX_TYPECHANGE)
            changes_in_index = 1;

        if (changes_in_index)
            break;
    }

    if (!changes_in_index) {
        err = -1;
        err_msg = git2r_err_nothing_added_to_commit;
        goto cleanup;
    }

    err = git_repository_index(&index, repository);
    if (err < 0)
        goto cleanup;

    if (!git_index_entrycount(index)) {
        err = -1;
        err_msg = git2r_err_nothing_added_to_commit;
        goto cleanup;
    }

    err = git_index_write_tree(&tree_oid, index);
    if (err < 0)
        goto cleanup;

    err = git_tree_lookup(&tree, repository, &tree_oid);
    if (err < 0)
        goto cleanup;

    count = LENGTH(parent_list);
    if (count) {
        parents = calloc(count, sizeof(git_commit*));
        if (NULL == parents) {
            err = -1;
            err_msg = git2r_err_alloc_memory_buffer;
            goto cleanup;
        }

        for (i = 0; i < count; i++) {
            git_oid oid;

            err = git_oid_fromstr(&oid, CHAR(STRING_ELT(parent_list, 0)));
            if (err < 0)
                goto cleanup;

            err = git_commit_lookup(&parents[i], repository, &oid);
            if (err < 0)
                goto cleanup;
        }
    }

    err = git_commit_create(&commit_id,
                            repository,
                            "HEAD",
                            sig_author,
                            sig_committer,
                            NULL,
                            CHAR(STRING_ELT(message, 0)),
                            tree,
                            count,
                            (const git_commit**)parents);
    if (err < 0)
        goto cleanup;

    err = git_commit_lookup(&new_commit, repository, &commit_id);
    if (err < 0)
        goto cleanup;

    PROTECT(sexp_commit = NEW_OBJECT(MAKE_CLASS("git_commit")));
    init_commit(new_commit, repo, sexp_commit);

cleanup:
    if (sig_author)
        git_signature_free(sig_author);

    if (sig_committer)
        git_signature_free(sig_committer);

    if (index)
        git_index_free(index);

    if (status)
        git_status_list_free(status);

    if (tree)
        git_tree_free(tree);

    if (repository)
        git_repository_free(repository);

    if (parents) {
        for (i = 0; i < count; i++) {
            if (parents[i])
                git_commit_free(parents[i]);
        }
        free(parents);
    }

    if (new_commit)
        git_commit_free(new_commit);

    UNPROTECT(1);

    if (err < 0) {
        if (err_msg) {
            error(err_msg);
        } else {
            const git_error *e = giterr_last();
            error("Error %d/%d: %s\n", err, e->klass, e->message);
        }
    }

    return sexp_commit;
}

/**
 * Get the tree pointed to by a commit
 *
 * @param commit S4 class git_commit
 * @return S4 class git_tree
 */
SEXP commit_tree(SEXP commit)
{
    int err;
    SEXP result = R_NilValue;
    SEXP hex;
    git_commit *commit_obj = NULL;
    git_oid oid;
    git_repository *repository = NULL;
    git_tree *tree = NULL;

    repository = get_repository(commit);
    if (!repository)
        error(git2r_err_invalid_repository);

    hex = GET_SLOT(commit, Rf_install("hex"));
    git_oid_fromstr(&oid, CHAR(STRING_ELT(hex, 0)));
    err = git_commit_lookup(&commit_obj, repository, &oid);
    if (err < 0)
        goto cleanup;

    err = git_commit_tree(&tree, commit_obj);
    if (err < 0)
        goto cleanup;

    PROTECT(result = NEW_OBJECT(MAKE_CLASS("git_tree")));
    init_tree((git_tree*)tree, result);

cleanup:
    if (commit_obj)
        git_commit_free(commit_obj);

    if (tree)
        git_tree_free(tree);

    if (repository)
        git_repository_free(repository);

    if (R_NilValue != result)
        UNPROTECT(1);

    if (err < 0)
        error("Error: %s\n", giterr_last()->message);

    return result;
}

/**
 * Init slots in S4 class git_commit
 *
 * @param source a commit object
 * @param repo S4 class git_repository that contains the blob
 * @param dest S4 class git_commit to initialize
 * @return void
 */
void init_commit(const git_commit *source, SEXP repo, SEXP dest)
{
    const char *message;
    const char *summary;
    const git_signature *author;
    const git_signature *committer;
    char hex[GIT_OID_HEXSZ + 1];

    git_oid_fmt(hex, git_commit_id(source));
    hex[GIT_OID_HEXSZ] = '\0';
    SET_SLOT(dest,
             Rf_install("hex"),
             ScalarString(mkChar(hex)));

    author = git_commit_author(source);
    if (author) {
        SEXP sexp_author;

        PROTECT(sexp_author = NEW_OBJECT(MAKE_CLASS("git_signature")));
        init_signature(author, sexp_author);
        SET_SLOT(dest, Rf_install("author"), sexp_author);
        UNPROTECT(1);
    }

    committer = git_commit_committer(source);
    if (committer) {
        SEXP sexp_committer;

        PROTECT(sexp_committer = NEW_OBJECT(MAKE_CLASS("git_signature")));
        init_signature(committer, sexp_committer);
        SET_SLOT(dest, Rf_install("committer"), sexp_committer);
        UNPROTECT(1);
    }

    summary = git_commit_summary(source);
    if (summary) {
        SET_SLOT(dest,
                 Rf_install("summary"),
                 ScalarString(mkChar(summary)));
    }

    message = git_commit_message(source);
    if (message) {
        SET_SLOT(dest,
                 Rf_install("message"),
                 ScalarString(mkChar(message)));
    }

    SET_SLOT(dest,
             Rf_install("path"),
             duplicate(GET_SLOT(repo, Rf_install("path"))));
}