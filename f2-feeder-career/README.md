# F2 Feeder Career 2026

Addon do Swofty Launcher (F1 25 com a temporada 2026) com duas Driver Careers que começam na Fórmula 2. Cada uma tem seu próprio tile na linha **Addons** da escolha de carreira, seu modo de jogo e seus saves. A carreira 2026 do jogo não é alterada.

| Tile | F2 | Depois | Situação |
|---|---|---|---|
| F2 2025 > F1 2026 | F2 com o grid `f2_2026` e o calendário 2025 | F1 2026 | abre na 0.4.0; calendário da 0.4.9 não testado |
| F2 2026 > F1 2027 | F2 com o grid `f2_2026` e o calendário 2026 com Madri | F1 com os carros de 2026 (ainda datada 2026) | Melbourne 1/14 na 0.4.9 |

`db/career.json` e `lang/career.json` são gerados por `tools/gen.py`. Mude o gerador, não os arquivos.

## Como funciona

**Tiles** (`native/src/f2_feeder.cpp`): dois *career contents* do core (`swofty/career_content.hpp`), com os grupos `f1_career_f2_25` (sufixo `FA26`) e `f1_career_f2_26` (sufixo `FB27`). `Game_CareerWizard_StartsInF1` responde "não" para esses dois grupos, e o wizard mostra Skip to F1 / Start in F2.

**Cada carreira** é montada como a do Progressive Career, com cópias das linhas da carreira 2026 do jogo:
- linhas dos 8 grupos de dados e a entrant list do grupo;
- `shared_components`, o campeonato de F1 (`F1_driver_career_f2_25`/`_26`, statId 38/39) e o modo (`driver_career_f2_25`/`_26`, statId 83/84), cada um com seu `save_data.name`;
- a lista `F2_Default` (a F2, depois a F1) e o campeonato de F2 como segunda referência, igual ao `driver_career_25`;
- listas `F1CAREER_`, `F2CAREER_` e `ICONS_` no content set 2026.

**As duas carreiras correm a F2 no `F2_career_23`**, o único campeonato de F2 que nenhum modo do F1 25 usa (cópias de campeonato de F2 crasharam). O grid fica no campeonato, então as duas usam o `f2_2026`. O calendário fica no setup `F2_Default` de cada modo:
- A: `F2_career_23`, com ano 2025;
- B: `F2_career_21`, reformatado igual ao de F2 2024, com ano 2026.

A cena do Start in F2 e a tela Official Driver usam pilotos de 2026 (Câmara, Herta, Montoya). O visual dos pilotos é buscado pelo ano da carreira, e pilotos que só correram em 2025 não aparecem.

**Calendários:** o `season_0.track_event_list` do event list de cada carreira nomeia a lista da temporada (`F2_2025_full`, `F2_2026_full`). Para `season_2025` e `season_2026` há linhas de duração (Long/Medium/Short) e `track_selections` de F2, além das listas curtas `F2_202x_8` e `F2_202x_3`. As linhas da F2 2024 ficam só para 2023 e 2024.
- As datas das rodadas vêm das race weekends (`schedule.all_track_events`): `rw_<pista>` tem data de 2025 e `rw_<pista>_26` de 2026. A F2 2026 tem as suas próprias, `rw_f2_<pista>_26` (stat_id 137–150), cópias das de 2026.
- Antes do primeiro carregamento de pista, o jogo refaz as etapas da temporada (`0x140bc9360`). Ele mantém só os eventos do calendário cujo `stat_id` está na lista `ComTrackList_0.default_track_list` do campeonato, na ordem dessa lista. No `F2_career_23` ela era a `F2_2024_full`: a temporada corria na ordem de 2024 (Bahrein primeiro, sem Madri, 13 rodadas), e com os eventos de 2026 ficava sem etapas e crashava. O addon aponta esse campo para `F2_feeder_order`, com os eventos de 2025 e de 2026. Cada calendário só tem os do seu ano. A F1 da carreira B ainda é datada de 2026: `season_2027` não existe em `game_setup.seasons` e crashou o boot (0x142a0ddb0).

**Saves:** cada carreira tem seu próprio container de save. Sem o addon, o launcher guarda esses containers e os escreve de volta sem mudança, então dá para deixar o Steam Cloud ligado.

## O que aprendemos (para quem continuar)

- **Uma cópia de `championships.Championship` derruba o jogo no boot**, em `Game_ModeChampionship_Build`, mesmo quando só o nome muda. Use um campeonato que já existe e não é usado.
- **Um campeonato de F1 não vira F2 pelo banco.** O `F1_career_23` com `game_setup_modes f2` crasha ao entrar no modo (`0xcb7330`, sob `Game_Game_SetCurrentGameMode`). O jogo ainda cria para ele o componente de P&D `development_events`, e na F2 nada liga esse componente. Não resolveu limpar `save_data`, tirar o `OptionsSystemConfiguration`, trocar o `statId`, nem pôr o campeonato na lista de campeonatos de F2 (chave `0xbb1488669c224074`), separado ou tudo junto.
- **Lista de pilotos da tela Choose Driver:** a aba F2 mostra só quem está ao mesmo tempo na lista `career_f2_*_for_f1_*` e no grid do campeonato. A `career_f2_*` é a lista de escolha, não o grid de corrida.
- **Saves:** mudar quais campeonatos a carreira usa quebra os saves de carreiras criadas antes da mudança. O crash acontece em `Game_SaveData_DeserialiseContainer`, ao carregar o perfil. O Steam Cloud traz o save antigo de volta se estiver ligado.

## Build e teste

```bash
SWOFTY_REPO=/caminho/SwoftyLauncher native/build.sh
```

No launcher: instale a pasta, ligue o addon num perfil de teste e limpe `%LOCALAPPDATA%\Swofty\cache\overlay\<perfil>` antes de testar uma versão nova.
