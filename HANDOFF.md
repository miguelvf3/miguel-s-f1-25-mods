# Handoff: F2 Feeder Career (F1 25 + temporada 2026)

Contexto para continuar o trabalho numa sessão nova. Leia também `f2-feeder-career/README.md`.

## Objetivo

Addon do [Swofty Launcher](https://github.com/Swofty-Developments/SwoftyLauncher) com duas Driver Careers que começam na F2:
- **F2 2025 > F1 2026**
- **F2 2026 > F1 2027**

A carreira 2026 do jogo não traz o Start in F2. A carreira 2025 do jogo traz (F2 2024, depois F1 2025).

## Estado atual: 0.4.0

Os dois tiles aparecem na linha **Addons** e abrem com Skip to F1 / Start in F2. Na 0.4.0 o Start in F2 mostra os carros de 2026, e a tela Official Driver mostra Herta, Câmara e Montoya. As duas carreiras dividem o campeonato de F2 `F2_career_23` e o grid `f2_2026`; cada uma tem o próprio calendário.

Já funcionou no jogo, na 0.2.3, quando o mod ainda alterava a carreira 2026 do jogo:
- Start in F2 aparecia na carreira 2026.
- O grid da F2 2025 tinha os 22 carros certos.
- O calendário da F2 2025 vinha na ordem real (Austrália primeiro), com as versões de 3, 8 e 14 corridas.
- A cena do Start in F2 mostrava Fornaroli, Verschoor e Lindblad com carros de 2025.
- A carreira 2025 continuava com a F2 2024.

Na 0.3.x tudo isso foi levado para tiles próprios, para não tocar mais nos saves da carreira 2026 do jogo.

Ainda não testado:
- a passagem da F2 para a F1 no fim da temporada (dá para testar rápido com a temporada de 3 corridas);
- a carreira B inteira.

## Como testar uma versão

1. Ligue o addon `f2-feeder-career/` no perfil `test` do launcher.
2. Limpe o cache: `Remove-Item -Recurse -Force "$env:LOCALAPPDATA\Swofty\cache\overlay\test"`
3. Inicie pelo launcher.
4. Logs: `swofty\loader.log` e `swofty\crash-*.txt` na pasta do jogo (hoje `C:\Program Files (x86)\Steam\steamapps\common\F1 25`), também copiados em `%LOCALAPPDATA%\Swofty\sessions\<data>\game-logs`, e `%LOCALAPPDATA%\Swofty\state\wrapper-run.log` para o build.
5. Banco do jogo: `swofty.exe db get "<caminho>"` e `swofty.exe db search <texto>`, na pasta `bin` do launcher. `swofty.exe build --profile test --side-data` só lista os achados depois de apagar o cache.

## O que aprendemos (não repita)

- **Copiar um `championships.Championship` de F2 derruba o boot** em `Game_ModeChampionship_Build`, mesmo com só o nome trocado. Cópias do campeonato de F1 (`F1_driver_career_26`) funcionam, como faz o Progressive. Para a F2, reaproveite campeonatos que nenhum modo do F1 25 usa:
  - `F2_career_23`, usado só pelos modos de F1 23/24;
  - `F1_career_23`, convertido em F2.
- **Entrant lists:**
  - O `stat_id` indexa um registro que termina no maior id original (104). Ids acima disso não são encontrados. Ids livres abaixo de 105: 2–12, 14, 15, 17–29, 31–37, 39, 53 e 68. A 0.3.x usa 53 e 68.
  - O jogo só carrega uma lista que esteja em `content_sets.content_set[_metadata_=2026].available_entrant_lists_0.entrant_lists`, ou que seja referenciada por uma lista carregada.
- **`add-row` só copia campos que o catálogo do launcher conhece.**
  - Para o modo, o campeonato e os grupos, use o mapa `fields` (`tools/fields.json`, tirado do Progressive).
  - Em listas aninhadas sem `_instancecount_`, como a `championship_setups` do `driver_career_26`, crie os campos com `set` + `create` + `type`.
- **Calendário da F2:**
  - O jogo escolhe a lista de pistas e a ordem pelo **ano das datas** do event list do campeonato (`season_2025`, `season_2026`), usando as linhas de `schedule.dynamic_event_data.default_ui_track_list_data`.
  - A ordem das rodadas vem de `track_selections`. Sem uma linha de F2 para o ano, o jogo usa a ordem da `F2_2024_full`.
  - `season_2027` não existe em `game_setup.seasons`, e colocar esse valor numa lista `seasons` crasha o boot (`0x142a0ddb0`). Por isso a F1 da carreira B ainda é datada 2026.
- **Start in F2:**
  - `Game_CareerWizard_StartsInF1` responde "sim" para a carreira 2026. O mod responde "não" para os grupos dele, mas **não dentro de `FeTile_Route_CareerItem`**: lá o caminho "não" lê um objeto que os tiles de addon não têm. No router o mod mantém a resposta original e troca a rota com `wz::tile_route(group, "career_wizard_driver_career")`.
- **Tela Choose Driver:** a aba F2 mostra só quem está ao mesmo tempo na lista `F2CAREER_<sufixo>` (por exemplo `career_f2_2025_for_f1_2026`) e no grid do campeonato. Pilotos que já estão na F1 do ano seguinte não aparecem; é assim no jogo original também.
- **Saves:**
  - Mudar quais campeonatos um modo usa quebra as carreiras salvas antes. O crash acontece em `Game_SaveData_DeserialiseContainer`, ao carregar o perfil.
  - O Steam Cloud traz o save antigo de volta. Para limpar: desligue a nuvem, apague `Steam\userdata\1013098445\3059520\remote\*`, crie um save novo e religue escolhendo a cópia local.
  - Com tiles próprios, cada carreira tem seu container (`gm_driver_career_f2_25`/`_26`). O launcher guarda esses containers quando o addon está desligado.
- **A fonte do jogo não tem "→"**; use ">".
- **Não converta campeonato de F1 em F2** (0.3.5 a 0.3.12, todas crasharam em `0xcb7330`). O objeto vazio era o componente `development_events` do campeonato. O crash report em `swofty\crash-*.txt` guarda a memória em volta dos registradores, e foi ali que o nome apareceu: o dump completo (`SWOFTY_CRASH_DUMP=full` em `launch.env` no `config.json`) precisa de dezenas de GB livres.

## Próximos passos

1. Conferir na carreira B que o calendário de F2 é o de 2026 (com Madri), e não o `F2_career_23` do campeonato.
2. Testar a passagem F2 > F1 com a temporada de 3 corridas.
3. Ajustar o título e a descrição da carreira A, que agora tem o grid de 2026.
4. O ano 2027 na F1 da carreira B precisa de outra solução, como uma linha em `game_setup.seasons` (risco: um statId acima de 31 trava o boot) ou troca de textos no código nativo, como faz o Progressive.
5. Sepang no lugar do Bahrein: o usuário já baixou um mod de pista.
6. Escrever uma nota no knowledge base do universal-modder (`um kb new`). Abrir PR só com o OK do usuário.

## Ferramentas

- `f2-feeder-career/tools/gen.py`: gera `db/career.json` e `lang/career.json`. Edite o gerador, não os JSON.
- `f2-feeder-career/native/build.sh`: compila o `f2_feeder.dll`. Precisa de zig e de um clone do SwoftyLauncher; o caminho vai em `SWOFTY_REPO`.
- `tools/dump-career-db.ps1`: despeja calendários, campeonatos e modos do banco do jogo com `swofty db get`. É lento: cerca de 500 chamadas.
- No SwoftyLauncher, as referências mais úteis:
  - `addons/progressive` (carreira como tile de addon);
  - `re/db/functions.tsv` (funções do exe);
  - `tools/gamedb/catalog/keys.tsv.gz` (chaves do banco);
  - `docs/reference/std/career-content.md` e `career-wizard.md`.
