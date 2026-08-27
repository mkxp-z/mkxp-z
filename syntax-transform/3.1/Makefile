SYNTAX_TRANSFORM_PATCHES := \
	$(SYNTAX_TRANSFORM_DIR)/0000-prelude.patch \
	$(SYNTAX_TRANSFORM_DIR)/0001-space-before-argument-parentheses.patch \
	$(SYNTAX_TRANSFORM_DIR)/0002-hash-args.patch \
	$(SYNTAX_TRANSFORM_DIR)/0003-then-do-colon.patch \
	$(SYNTAX_TRANSFORM_DIR)/0004-block-without-parameter-definition.patch \
	$(SYNTAX_TRANSFORM_DIR)/0005-character-literal.patch \
	$(SYNTAX_TRANSFORM_DIR)/0006-else-without-rescue.patch \
	$(SYNTAX_TRANSFORM_DIR)/0007-block-variable-shadowing.patch \
	$(SYNTAX_TRANSFORM_DIR)/0008-variables-array.patch \
	$(SYNTAX_TRANSFORM_DIR)/0009-separation-of-positional-and-keyword-arguments.patch \
	$(SYNTAX_TRANSFORM_DIR)/0010-regexp-encoding.patch \
	$(SYNTAX_TRANSFORM_DIR)/0011-class-variable-overtaken.patch \
	$(SYNTAX_TRANSFORM_DIR)/0012-nil-true-false-constants.patch \
	$(SYNTAX_TRANSFORM_DIR)/0013-ruby-version.patch \
	$(SYNTAX_TRANSFORM_DIR)/0014-singleton-class-superclass.patch \
	$(SYNTAX_TRANSFORM_DIR)/0015-hash-select.patch \
	$(SYNTAX_TRANSFORM_DIR)/0016-multiple-values-for-a-block-parameter.patch \
	$(SYNTAX_TRANSFORM_DIR)/0017-array-hash-to-s.patch \
	$(SYNTAX_TRANSFORM_DIR)/0018-to-proc.patch \
	$(SYNTAX_TRANSFORM_DIR)/0019-toplevel-constant.patch \
	$(SYNTAX_TRANSFORM_DIR)/0020-class-variable-access-from-toplevel.patch \
	$(SYNTAX_TRANSFORM_DIR)/0021-kernel-match.patch \
	$(SYNTAX_TRANSFORM_DIR)/0022-string-aref.patch \
	$(SYNTAX_TRANSFORM_DIR)/0023-integer-fixnum-bignum.patch \
	$(SYNTAX_TRANSFORM_DIR)/0024-module-constants.patch \
	$(SYNTAX_TRANSFORM_DIR)/0025-exception-equal.patch \
	$(SYNTAX_TRANSFORM_DIR)/0026-superclass-mismatch.patch \
	$(SYNTAX_TRANSFORM_DIR)/0027-kernel-proc.patch \
	$(SYNTAX_TRANSFORM_DIR)/0028-numeric-div.patch \
	$(SYNTAX_TRANSFORM_DIR)/0029-incomplete-format-specifier.patch \
	$(SYNTAX_TRANSFORM_DIR)/0030-invalid-keyword.patch  \
	$(SYNTAX_TRANSFORM_DIR)/0031-retry-redo.patch \
	$(SYNTAX_TRANSFORM_DIR)/0032-basic-object-initialize.patch \
	$(SYNTAX_TRANSFORM_DIR)/0033-nil-true-false-to-s.patch \
	$(SYNTAX_TRANSFORM_DIR)/0034-command-trailing-comma.patch \
	$(SYNTAX_TRANSFORM_DIR)/0035-splat-assignment.patch \
	$(SYNTAX_TRANSFORM_DIR)/0036-brace-after-array-literal-arg.patch \

HAVE_SYNTAX_TRANSFORM ?= 1

$(RUBY_SRC)/syntax-transform: $(RUBY_SRC)/configure.ac
ifeq ($(HAVE_SYNTAX_TRANSFORM),1)
ifeq ($(patsubst MINGW%,,$(firstword $(shell uname))),)
	cd $(RUBY_SRC) && for patch in $(SYNTAX_TRANSFORM_PATCHES); do echo "Applying $$patch"; patch -p1 -i $(PWD)/$$patch || exit 1; done
else
	cd $(RUBY_SRC) && for patch in $(SYNTAX_TRANSFORM_PATCHES); do echo "Applying $$patch"; $(GIT) apply --whitespace=fix $(PWD)/$$patch || exit 1; done
endif
endif
	touch $(RUBY_SRC)/syntax-transform
