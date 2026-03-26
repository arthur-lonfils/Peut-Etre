CC       = gcc
SRCDIR   = src
BUILDDIR = build
SOURCES  = $(wildcard $(SRCDIR)/*.c)
OBJECTS  = $(SOURCES:$(SRCDIR)/%.c=$(BUILDDIR)/%.o)
TARGET   = peut-etre
VERSION ?= $(shell git describe --tags --abbrev=0 2>/dev/null | sed 's/^v//' || echo "0.0.0")
CFLAGS   = -Wall -Wextra -pedantic -std=c11 -g -O1 -DPE_VERSION=\"$(VERSION)\"

.PHONY: all clean test setup vscode-ext changelog

all: $(TARGET)

$(TARGET): $(OBJECTS)
	$(CC) $(CFLAGS) -o $@ $^

$(BUILDDIR)/%.o: $(SRCDIR)/%.c | $(BUILDDIR)
	$(CC) $(CFLAGS) -c -o $@ $<

$(BUILDDIR):
	mkdir -p $(BUILDDIR)

test: $(TARGET)
	./tests/run_tests.sh

setup:
	@echo "Installing git hooks..."
	@ln -sf ../../hooks/commit-msg .git/hooks/commit-msg
	@ln -sf ../../hooks/pre-commit .git/hooks/pre-commit
	@echo "  ✓ commit-msg hook installed"
	@echo "  ✓ pre-commit hook installed"
	@echo ""
	@echo "Git hooks are ready. Bon courage."

vscode-ext: $(TARGET)
	@echo "Generating TextMate grammar..."
	@./$(TARGET) --generate-tmLanguage > vscode-extension/peut-etre.tmLanguage.json
	@echo "Packaging VS Code extension..."
	@cd vscode-extension && npx @vscode/vsce package --allow-missing-repository
	@echo ""
	@echo "Install with:"
	@echo "  code --install-extension vscode-extension/peut-etre-$(VERSION).vsix"

changelog:
	@git cliff --output CHANGELOG.md
	@echo "CHANGELOG.md updated."

clean:
	rm -rf $(BUILDDIR) $(TARGET) vscode-extension/*.vsix
