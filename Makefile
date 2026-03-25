CC       = gcc
CFLAGS   = -Wall -Wextra -pedantic -std=c11 -g -O1
SRCDIR   = src
BUILDDIR = build
SOURCES  = $(wildcard $(SRCDIR)/*.c)
OBJECTS  = $(SOURCES:$(SRCDIR)/%.c=$(BUILDDIR)/%.o)
TARGET   = peut-etre
VERSION  = 0.1.2

.PHONY: all clean test setup vscode-ext changelog release

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

release: $(TARGET) changelog vscode-ext
	@echo ""
	@echo "Release v$(VERSION) ready:"
	@echo "  1. git add CHANGELOG.md && git commit -m 'chore: update changelog'"
	@echo "  2. git tag -a v$(VERSION) -m 'v$(VERSION)'"
	@echo "  3. git push origin main v$(VERSION)"
	@echo "  4. gh release create v$(VERSION) ./peut-etre ./vscode-extension/peut-etre-$(VERSION).vsix"

clean:
	rm -rf $(BUILDDIR) $(TARGET) vscode-extension/*.vsix
