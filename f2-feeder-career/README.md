# F2 Feeder Career 2026

Addon do Swofty Launcher (F1 25 com a temporada 2026) que traz de volta o **Start in F2** na Driver Career 2026: uma temporada de F2 2025 e depois a F1 2026. A carreira 2025 fica como no jogo original (F2 2024, depois F1 2025).

| Modo | F2 | Depois | Situação |
|---|---|---|---|
| Carreira 2026 | F2 2025 (grid `f2_2025`, calendário 2025) | F1 2026 | testado até a classificação da 1ª etapa; falta testar a passagem para a F1 |
| F2 2026 → F1 2027 | F2 2026 (`f2_2026`) | F1 "2027" | ainda não feito |

## O que o addon muda

**Wizard** (`native/src/f2_feeder.cpp`): `Game_CareerWizard_StartsInF1` responde "não" para o grupo `f1_career_2026`, e o wizard volta a mostrar Skip to F1 / Start in F2.

**Campeonato** (`db/career.json`): a F2 da carreira 2026 usa o `F2_career_23`, um campeonato de F2 que só os modos de carreira do F1 23 e do F1 24 usam e que o F1 25 não oferece mais. Ele recebe:
- o grid `f2_2025` e o `setup_data` `f1_career_2026`;
- o calendário `F2_career_23` com o ano 2025, e por isso a temporada de F2 é `season_2025`.

`driver_career_26` ganha a lista `F2_Default` (`F2_career_23`, depois `F1_driver_career_26`) e o `F2_career_23` como segunda referência de campeonato, igual ao `driver_career_25` com o `F2_2024_career`.

**Calendário:**
- Listas novas `F2_2025_8` e `F2_2025_3`: as listas curtas de 2024 na ordem de 2025, com Melbourne primeiro. A completa, `F2_2025_full`, já existe no jogo.
- Linhas de duração de temporada `F2_2025_Long/Medium/Short` para `season_2025`. As de 2024 ficam só para `season_2023` e `season_2024`.
- Linhas `track_selections` de F2 (`F2_2024` e `F2_2025`). A ordem das rodadas vem daqui: sem uma linha de F2, o jogo usava a ordem da `F2_2024_full`.

**Cena do Start in F2** (`SpawnedF2` do grupo `f1_career_2026`): carros `f2_car_25` com Fornaroli (Invicta), Verschoor (MP) e Lindblad (Campos).

## O que aprendemos (para quem continuar)

- **Uma cópia de `championships.Championship` derruba o jogo no boot**, em `Game_ModeChampionship_Build`, mesmo quando só o nome muda. Use um campeonato que já existe e não é usado.
- **Lista de pilotos da tela Choose Driver:** a aba F2 mostra só quem está ao mesmo tempo na lista `career_f2_*_for_f1_*` e no grid do campeonato. A `career_f2_*` é a lista de escolha, não o grid de corrida.
- **Saves:** mudar quais campeonatos a carreira usa quebra os saves de carreiras criadas antes da mudança. O crash acontece em `Game_SaveData_DeserialiseContainer`, ao carregar o perfil. O Steam Cloud traz o save antigo de volta se estiver ligado.

## Build e teste

```bash
SWOFTY_REPO=/caminho/SwoftyLauncher native/build.sh
```

No launcher: instale a pasta, ligue o addon num perfil de teste e limpe `%LOCALAPPDATA%\Swofty\cache\overlay\<perfil>` antes de testar uma versão nova.
