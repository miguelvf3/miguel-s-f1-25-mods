# F2 Feeder Career 2026

Addon do Swofty Launcher (F1 25 com a temporada 2026) com duas Driver Careers que começam na Fórmula 2. Cada uma tem seu próprio tile na linha **Addons** da escolha de carreira, seu modo de jogo e seus saves. A carreira 2026 do jogo não é alterada.

| Tile | F2 | Depois | Situação |
|---|---|---|---|
| F2 2025 → F1 2026 | F2 2025 (grid `f2_2025`, calendário 2025) | F1 2026 | na 0.2.3, testado até a 1ª etapa |
| F2 2026 → F1 2027 | F2 2026 (grid `f2_2026`, calendário 2026 com Madri) | F1 2027 com os carros de 2026 | novo na 0.3.0 |

`db/career.json` e `lang/career.json` são gerados por `tools/gen.py`. Mude o gerador, não os arquivos.

## Como funciona

**Tiles** (`native/src/f2_feeder.cpp`): dois *career contents* do core (`swofty/career_content.hpp`), com os grupos `f1_career_f2_25` (sufixo `FA26`) e `f1_career_f2_26` (sufixo `FB27`). `Game_CareerWizard_StartsInF1` responde "não" para esses dois grupos, e o wizard mostra Skip to F1 / Start in F2.

**Cada carreira** é montada como a do Progressive Career, com cópias das linhas da carreira 2026 do jogo:
- linhas dos 8 grupos de dados e a entrant list do grupo;
- `shared_components`, o campeonato de F1 (`F1_driver_career_f2_25`/`_26`, statId 38/39) e o modo (`driver_career_f2_25`/`_26`, statId 83/84), cada um com seu `save_data.name`;
- a lista `F2_Default` (a F2, depois a F1) e o campeonato de F2 como segunda referência, igual ao `driver_career_25`;
- listas `F1CAREER_`, `F2CAREER_` e `ICONS_` no content set 2026.

**A F2 de cada carreira usa um campeonato do jogo que nenhum modo do F1 25 usa** (cópias de campeonato de F2 crasharam):
- A: `F2_career_23`, com o grid `f2_2025` e o calendário com ano 2025;
- B: `F1_career_23` transformado em F2 (`game_setup_modes f2`), com o grid `f2_2026` e o calendário `F2_career_21` reformatado igual ao de F2 2024, com ano 2026.

**Calendários:** o jogo escolhe as pistas e a ordem pelo ano da temporada (as datas do calendário do campeonato). Para `season_2025` e `season_2026` há linhas de duração (Long/Medium/Short) e `track_selections` de F2, além das listas curtas `F2_202x_8` e `F2_202x_3`. As linhas da F2 2024 ficam só para 2023 e 2024. A F1 2027 da carreira B tem o ano 2027, e `season_2027` é acrescentado às linhas da F1 2026.

**Saves:** cada carreira tem seu próprio container de save. Sem o addon, o launcher guarda esses containers e os escreve de volta sem mudança, então dá para deixar o Steam Cloud ligado.

## O que aprendemos (para quem continuar)

- **Uma cópia de `championships.Championship` derruba o jogo no boot**, em `Game_ModeChampionship_Build`, mesmo quando só o nome muda. Use um campeonato que já existe e não é usado.
- **Lista de pilotos da tela Choose Driver:** a aba F2 mostra só quem está ao mesmo tempo na lista `career_f2_*_for_f1_*` e no grid do campeonato. A `career_f2_*` é a lista de escolha, não o grid de corrida.
- **Saves:** mudar quais campeonatos a carreira usa quebra os saves de carreiras criadas antes da mudança. O crash acontece em `Game_SaveData_DeserialiseContainer`, ao carregar o perfil. O Steam Cloud traz o save antigo de volta se estiver ligado.

## Build e teste

```bash
SWOFTY_REPO=/caminho/SwoftyLauncher native/build.sh
```

No launcher: instale a pasta, ligue o addon num perfil de teste e limpe `%LOCALAPPDATA%\Swofty\cache\overlay\<perfil>` antes de testar uma versão nova.
