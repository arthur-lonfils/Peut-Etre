<p align="center">
  <img src="icons/peut-etre_logo-transparent_bg.png" alt="Peut-Être Logo" width="200"/>
  <h1 align="center">Peut-Être</h1>
  <p align="center"><strong>The Non-Committal Programming Language</strong></p>
  <p align="center">
    A French esoteric language where nothing is guaranteed.<br/>
    Variables might change. Conditions evaluate based on vibes. Loops get tired and quit.
  </p>
  <p align="center">
    <img alt="Language" src="https://img.shields.io/badge/language-C11-blue"/>
    <img alt="License" src="https://img.shields.io/badge/license-MIT-green"/>
    <img alt="Vibes" src="https://img.shields.io/badge/vibes-immaculate-ff69b4"/>
    <img alt="Reliability" src="https://img.shields.io/badge/reliability-peut--être-orange"/>
  </p>
</p>

---

## Philosophy

**Peut-Être** (French for "Maybe") is a programming language built on one core principle: *commitment is overrated*.

The architectural joke? We use **C** — the most disciplined, strictly-typed, memory-managed language in existence — to build a language that is fundamentally lazy, probabilistic, and entirely based on vibes.

```
genre x = 42;
genre y = x + 8;

si_ca_te_tente (x > 40) {
    lache_un_com("x est plutôt grand, probablement");
} sinon_tant_pis {
    lache_un_com("x est pas ouf");
}
```

> **Translation**: *Declare x as 42. If you feel like it, check if x > 40. If so, print "x is pretty big, probably." Otherwise, whatever, print "x isn't great."*

---

## The Chaos Engine

Every Peut-Être program runs through the **Chaos Engine** — three probabilistic behaviors injected at runtime:

| Behavior | Trigger | Effect |
|---|---|---|
| **Math Apathy** | Any `+` `-` `*` `/` | 5% chance the result is off by ±1. *"Honnêtement, à un près, on va pas chipoter."* |
| **Conditional Laziness** | Any `si_ca_te_tente` (if) | 5% chance it evaluates to `false` regardless. *"Flemme de vérifier."* |
| **Loop Exhaustion** | After 20 iterations of `jusqu_a_ce_qu_on_en_ait_marre` (while) | 10% chance per cycle of forced break. *"C'est bon, ça tourne en rond ce truc, j'arrête."* |

---

## Quick Start

### Build

```bash
git clone git@github.com:arthur-lonfils/Peut-Etre.git
cd Peut-Etre
make
make setup    # install git hooks (conventional commits + pre-commit checks)
```

Requires `gcc` with C11 support. No external dependencies.

### Run

```bash
./peut-etre examples/bonjour.pt          # transpile & run (coming soon)
./peut-etre --lex examples/bonjour.pt    # see the token stream
./peut-etre --ast examples/bonjour.pt    # see the AST
```

---

## The Lexicon

Keywords are loaded from `dictionnaire.json` at runtime — add your own regional slang without touching C.

### Variables & Functions

| Peut-Être | Meaning | Literal Translation |
|---|---|---|
| `genre x = 5;` | `var x = 5;` | "like, x = 5" |
| `en_gros y = 10;` | `var y = 10;` | "roughly, y = 10" |
| `petit_truc_qui_fait f(x)` | `function f(x)` | "little thing that does f(x)" |
| `on_verra_bien x;` | `return x;` | "we'll see" |
| `lache_un_com("salut");` | `print("salut");` | "drop a comment" |

### Control Flow

| Peut-Être | Meaning | Literal Translation |
|---|---|---|
| `si_ca_te_tente (cond)` | `if (cond)` | "if you feel like it" |
| `sur_un_malentendu (cond)` | `else if (cond)` | "on a misunderstanding" |
| `sinon_tant_pis` | `else` | "otherwise, whatever" |
| `jusqu_a_ce_qu_on_en_ait_marre (cond)` | `while (cond)` | "until we get sick of it" |
| `c_est_bon_laisse_tomber;` | `break;` | "alright, drop it" |
| `flemme_passe_au_suivant;` | `continue;` | "too lazy, skip to next" |

### Booleans

| Peut-Être | Value | Literal Translation |
|---|---|---|
| `carrement` | `true` (100%) | "absolutely" |
| `mort` | `false` (0%) | "dead" |
| `ouais_vite_fait` | 75% true | "yeah, kinda" |
| `bof` | 25% true | "meh" |
| `c_est_pas_faux` | 50% true | "it's not false" (Kaamelott ref) |
| `nada` | `null` | "nothing" |

### Types & Operators

| Peut-Être | Meaning |
|---|---|
| `blabla` | string type |
| `un_chiffre` | number type |
| `un_tas_de_trucs` | array type |
| `n_importe_quoi` | any type |
| `kif_kif` | `==` (with 5% fuzzy match) |
| `pas_trop_pareil` | `!=` |
| `plus_gros_que` | `>` |
| `moins_ouf_que` | `<` |
| `et_vas_y_aussi` | `&&` |
| `ou_alors_bref` | `\|\|` |

### Async & Error Handling (TypeScript target)

| Peut-Être | Meaning |
|---|---|
| `quand_j_aurai_le_temps` | `async` |
| `pause_cafe` | `await` (10% chance to hang forever) |
| `on_tente_un_truc` | `try` |
| `oups_my_bad` | `catch` |
| `bref_on_avance` | `finally` |

---

## DevEx Tooling

Because every serious language needs terrible tooling.

### The Lazy Linter (`--lint`)

Doesn't find bugs — it complains if your code is *too clean*:

```bash
./peut-etre --lint examples/bonjour.pt
# [LINT] examples/bonjour.pt — Bon, ça a l'air suffisamment n'importe quoi. Ça passe.
```

If there are no chaotic features or the file is over 50 lines:
> *"Warning: Code trop propre ou trop long. Flemme de lire."*

### The Anarchist Formatter (`--format`)

Randomly misaligns brackets, swaps spaces and tabs:

```bash
./peut-etre --format examples/bonjour.pt
# ...chaotically reformatted code...
# /* Voilà, c'est 'artistique' maintenant. */
```

### VS Code Extension

Syntax highlighting, snippets, and bracket matching for `.pt` files:

```bash
make vscode-ext                                              # package the extension
code --install-extension vscode-extension/peut-etre-0.1.0.vsix  # install it
```

Or regenerate the grammar manually:

```bash
./peut-etre --generate-tmLanguage > vscode-extension/peut-etre.tmLanguage.json
```

---

## Multi-Target Transpilation

Peut-Être transpiles to **C** (default) or **TypeScript**:

```bash
./peut-etre --target c  examples/bonjour.pt   # C output (default)
./peut-etre --target ts examples/bonjour.pt   # TypeScript output
```

The C target compiles with `gcc` and runs immediately. The TypeScript target runs via `ts-node` — because if you're going to be non-committal, you might as well do it in the browser too.

---

## Custom Dictionaries

The keyword mappings live in `dictionnaire.json`. Want to add your own regional French slang? Just edit the file:

```json
{
    "VAR": ["genre", "en_gros", "wesh"],
    "IF": "si_ca_te_tente",
    "WHILE": "jusqu_a_ce_qu_on_en_ait_marre"
}
```

Use `--dict path/to/custom.json` to load a custom dictionary. No recompilation needed.

---

## Project Status

| Phase | Status |
|---|---|
| Lexer (dynamic dictionary, 49 token types) | Done |
| Parser (recursive-descent, Pratt expressions) | Done |
| Chaos Runtime (C + TypeScript) | Done |
| Code Generation (C + TypeScript backends) | Done |
| End-to-End Pipeline | Done |
| Test Suite (30 tests) | Done |
| VS Code Extension (syntax, snippets) | Done |

---

## Contributing

This is a joke language. Contributions should be funny, chaotic, or both. If your PR is too sensible, it will be rejected on principle.

---

## License

MIT — Do whatever you want. We're not committed to stopping you.
