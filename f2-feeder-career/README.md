# F2 Feeder Career 2026

Addon do Swofty Launcher (F1 25 com a temporada 2026) com uma Driver Career que começa na Fórmula 2 de 2026 e sobe para a F1. Ela tem um tile próprio na linha **Addons** da escolha de carreira, com seu próprio modo de jogo e seus próprios saves. A carreira 2026 do jogo não é alterada.

| Tile | F2 | Depois | Situação |
|---|---|---|---|
| F2 2026 > F1 2027 | Grid `f2_2026` e calendário de 2026, com Madri | F1 com os carros de 2026, o calendário de 2027 (24 etapas) e os títulos de 2027 | Melbourne 1/14 e a passagem F2 > F1 funcionam (0.4.9). O calendário de 2027 entrou na 0.6.0 e ainda não foi testado |

Até a 0.4.9 havia uma segunda carreira, F2 2025 > F1 2026 (grupo `f1_career_f2_25`). Ela saiu na 0.5.0.

`db/career.json` e `lang/career.json` são gerados por `tools/gen.py`. Mude o gerador, não os arquivos.

## Como funciona

**Tile** (`native/src/f2_feeder.cpp`): um *career content* do core (`swofty/career_content.hpp`), com o grupo `f1_career_f2_26` (sufixo `FB27`). `Game_CareerWizard_StartsInF1` responde "não" para esse grupo, e o wizard mostra Skip to F1 / Start in F2.

**A carreira** é montada como a do Progressive Career, com cópias das linhas da carreira 2026 do jogo:
- linhas dos 8 grupos de dados e a entrant list do grupo;
- `shared_components`, o campeonato de F1 (`F1_driver_career_f2_26`, statId 39) e o modo (`driver_career_f2_26`, statId 84), com seu `save_data.name`;
- a lista `F2_Default` (a F2, depois a F1) e o campeonato de F2 como segunda referência, igual ao `driver_career_25`;
- listas `F1CAREER_`, `F2CAREER_` e `ICONS_` no content set 2026.

**A F2 corre no `F2_career_23`**, o único campeonato de F2 que nenhum modo do F1 25 usa (cópias de campeonato de F2 crasharam). O grid fica no campeonato (`f2_2026`). O calendário fica no setup `F2_Default` do modo: o event list `F2_career_21`, reformatado igual ao de F2 2024, com ano 2026.

A cena do Start in F2 e a tela Official Driver usam pilotos de 2026 (Câmara, Herta, Montoya). O visual dos pilotos é buscado pelo ano da carreira, e pilotos que só correram em 2025 não aparecem.

**Calendário:** o `season_0.track_event_list` do event list nomeia a lista da temporada (`F2_2026_full`). Para `season_2026` há linhas de duração (Long/Medium/Short) e `track_selections` de F2, além das listas curtas `F2_2026_8` e `F2_2026_3`. As linhas da F2 2024 do jogo cobrem 2023 a 2025.
- As datas das rodadas vêm das race weekends (`schedule.all_track_events`): `rw_<pista>` tem data de 2025 e `rw_<pista>_26` de 2026. A F2 2026 tem as suas próprias, `rw_f2_<pista>_26` (stat_id 137–150), cópias das de 2026.
- Antes do primeiro carregamento de pista, o jogo refaz as etapas da temporada (`0x140bc9360`). Ele mantém só os eventos do calendário cujo `stat_id` está na lista `ComTrackList_0.default_track_list` do campeonato, na ordem dessa lista. No `F2_career_23` ela era a `F2_2024_full`: a temporada corria na ordem de 2024 (Bahrein primeiro, sem Madri, 13 rodadas), e com os eventos de 2026 ficava sem etapas e crashava. O addon aponta esse campo para `F2_feeder_order`, com os eventos da F2 2026. 
**F1 2027:** a primeira temporada de F1 corre o calendário de 2027 da FIA (`F1_2027_full`, 24 etapas) sobre os dados de 2026 do jogo. Portugal e Turquia correm em addons de pista, que o manifesto exige, porque o F1 25 não tem os arquivos desses circuitos: `mod.portimao` (Pajko) preenche a vaga `portimao` do jogo, e `track.istanbul_park` (Apoc & RK) cria a pista `istanbul_park`, copiada de Spa. A race weekend da Turquia é a de Spa com outra atividade, e o addon cria a atividade `istanbul_park` (`schedule.activities.activity_race_weekend`, cópia da de Spa, stat_id 814): sem ela a temporada deixava a rodada de fora (0.7.0, 23 etapas).
- As race weekends `rw_f1_<pista>_27` (stat_id 151–174; Portugal e Turquia por último, 173 e 174) são cópias das de 2026. Cada uma tem como data a quinta-feira da etapa de 2027, mas com o ano 2026, porque a temporada do jogo vai de 11/02 a 21/12/2026. Datar como 2027 exigiria uma linha `season_2027` (o jogo só tem `season_2023` a `season_2026`) e `season_2027` ao lado de `season_2026` em uns 40 campos.
- A lista é ordenada por data (`sort_by_date`), igual à `F1_2026_full`. As temporadas curtas `F1_2027_16` e `F1_2027_10` são as de 2026 em 2027, com Mônaco no lugar de Zandvoort.
- A F1 usa o event list do jogo (`F1_career_26`), e a lista sai da escolha de duração (Full Season ou Custom Season), como numa carreira normal. Há linhas de duração `F1_2027_Long/Medium/Short` e um `track_selections` `F1_2027` (o banco de pistas do Custom Season). O jogo pega a primeira linha que serve para o modo e a temporada, então, enquanto a carreira está em foco (o assistente de criação dela ou ela carregada), a DLL esconde as de 2026 (`Game_SeasonLength_ApplyPreset`; tipo da duração `0xff`, modos do banco de pistas `0`) e as devolve ao sair.
- Um event list próprio que nomeava `F1_2027_full` em cada temporada (0.6.0–0.6.3) montava as etapas de 2027, mas abria a negociação de contrato na primeira visita à Team HQ. Lá `Career_ContractRenewal_ReplaceOldPosition` deixava o jogador sem equipe (time `0x3fff`), e a Team HQ crashava (rastreamento Frida, comparado com a 0.5.0, que não assina contrato nenhum).
- Enquanto a carreira está carregada, a DLL troca os títulos dos GPs e o nome do campeonato para 2027, com a API `swofty::lang`. Os textos vêm de `native/src/titles_2027.inc`, gerado de `tools/titles_2026.json`, que é o `data/titles.json` do Progressive Career. As etapas da F2 usam outras chaves de texto (`lng_track_<código>_f2_title`).

**Saves:** a carreira tem seu próprio container de save (`driver_career_f2_26`). Sem o addon, o launcher guarda esses containers e os escreve de volta sem mudança, então dá para deixar o Steam Cloud ligado.

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
