# F2 Feeder Career 2026 (RASCUNHO, não testado no jogo)

Addon para o Swofty Launcher (F1 25 + conteúdo da temporada 2026) que traz de volta o **Start in F2** na Driver Career.

| Modo | Temporada de F2 | Depois | Situação |
|---|---|---|---|
| A | F2 2025 (`career_f2_2025_for_f1_2026`) | F1 2026 | rascunho neste addon |
| B | F2 2026 (`f2_2026`) | F1 "2027" | só planejado (veja abaixo) |

## Como o jogo faz em 2025 (base do mod)

Dados do catálogo do banco do jogo, em `SwoftyLauncher/tools/gamedb/catalog/keys.tsv.gz`:

- `game_modes.ModeChampionship[driver_career_25].championship_setups` tem duas listas:
  - `F1_Only` = [`F1_driver_career_25`]
  - `F2_Default` = [`F2_2024_career`, `F1_driver_career_25`]
- O conteúdo 2026 já tem `F2CAREER_26 → career_f2_2025_for_f1_2026`, e o `CustomTeamSetup` de 2026 já seleciona `career_f2_2025_for_f1_2026,f1_2026_driver_pool`. O grid de F2 do ano anterior já está pronto; falta só a temporada de F2 na carreira.
- `Game_CareerWizard_StartsInF1` (0x14078ddd0, `re/db/functions.tsv`) responde "sim" para o grupo `f1_career_2026`. Por isso o wizard pula o passo Skip to F1 / Start in F2 (opções 6 e 7).

## O que o rascunho faz

1. `db/career.json`:
   - clona o championship `F2_2024_career` como `F2_2025_career`, com o grid `career_f2_2025_for_f1_2026`;
   - clona a event list de F2 de 2024 (o calendário ainda é o de 2024);
   - adiciona a lista `F2_Default` em `driver_career_26` (F2 2025, depois F1 2026) e a referência ao championship.
2. `native/src/f2_feeder.cpp`: envolve `Game_CareerWizard_StartsInF1` para responder "não" em `f1_career_2026`, e assim o wizard volta a oferecer Start in F2.

## Pontos que não verifiquei (preciso do jogo para isso)

- **Seletores:** as tabelas aninhadas (`...championship_setups`, `...championships`) e o nome `F1_career_26` vêm do catálogo e do `gen.py` do Progressive. O `swofty build` confirma se cada `add-row` aplica.
- **Ordem das listas:** não sei se o jogo escolhe `F2_Default` e `F1_Only` pelo índice ou pelo nome. Se for pelo índice, a nova lista pode precisar ficar em primeiro.
- **Por que o StartsInF1 dá "sim" em 2026:** ele procura um registro de veículo com o campo `+0xc8`. Pode haver mais alguma coisa da F2 que o 2026 não tem, e nesse caso o jogo pode travar.
- **Datas:** a event list clonada ainda está com o calendário e as datas de 2024.
- **Promoção:** não verifiquei se a passagem F2 → F1 no fim da temporada (contratos com equipes de F1) funciona com `F1_driver_career_26`.

## Modo B (F2 2026 → F1 2027): plano

É um tile novo de career content, no mesmo padrão do Progressive Career (`swofty/career_content.hpp`):

- data group `f1_career_2027`, clonado de `f1_career_2026` (career_setup, accolades, leverage etc.);
- uma entrant list `career_f2_2026_for_f1_2027`, clonada de `career_f2_2025_for_f1_2026` com os pilotos de `f2_2026`;
- `F2_2026_career`, `F1_driver_career_27` e a mode `driver_career_27` com `F2_Default`;
- a F1 "2027" usaria os carros e grids de 2026. O ano nos títulos exige o mesmo truque nativo de títulos do Progressive (`titles.json`).

## Build e teste

```bash
SWOFTY_REPO=/caminho/SwoftyLauncher native/build.sh
```

Depois, no launcher: Addons → instalar a pasta, ligar o addon num perfil de teste e iniciar o jogo. No `swofty/loader.log`, procure `[f2-feeder]` e os avisos de `add-row`. Faça backup dos saves antes.
