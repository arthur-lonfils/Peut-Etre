# Peut-Être — VS Code Extension

Syntax highlighting, bracket matching, and snippets for the **Peut-Être** programming language.

## Features

- Syntax highlighting for all Peut-Être keywords (loaded from `dictionnaire.json`)
- Auto-closing brackets, quotes, and braces
- Code folding
- Snippets for common patterns (`genre`, `si_ca_te_tente`, `jusqu_a`, `petit_truc`, etc.)

## Install (from source)

```bash
# From the vscode-extension/ directory
npm install -g @vscode/vsce
vsce package
code --install-extension peut-etre-0.1.0.vsix
```

Or copy the folder to `~/.vscode/extensions/peut-etre/`.

## Snippets

| Prefix | Expands to |
|---|---|
| `genre` | `genre nom = valeur;` |
| `si_ca_te_tente` | if block |
| `si_sinon` | if/else block |
| `si_sur_sinon` | if/elif/else block |
| `jusqu_a` | while loop |
| `petit_truc` | function definition |
| `lache` | `lache_un_com(valeur);` |
| `on_verra` | `on_verra_bien valeur;` |
| `c_est_bon` | `c_est_bon_laisse_tomber;` |

## Regenerate Grammar

If you modify `dictionnaire.json`, regenerate the TextMate grammar:

```bash
./peut-etre --generate-tmLanguage > vscode-extension/peut-etre.tmLanguage.json
```
