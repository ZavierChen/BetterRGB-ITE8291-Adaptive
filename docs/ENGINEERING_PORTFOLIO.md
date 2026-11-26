# Engineering Projects | Zewen Chen

The projects below started with everyday questions about a laptop, a math class, and a stale chess suggestion. Each write-up explains what the project tries to solve, what was tested or checked, and where the result stops.

## BetterRGB: Make keyboard lighting responsive without changing firmware

**Question.** On a laptop with an ITE8291 controller, could an independent Windows app make the stock lighting experience smoother without touching BIOS, embedded-controller firmware, or drivers?

**Design choice.** Build on TheYamo's open-source work, adapt it for the target keyboard, and keep control in a Windows application. Separate frame generation from hardware output so the application can reduce the send rate when output slows down, rather than insisting on the same update rate under all conditions.

**Evidence and limits.** The repository documents the verified hardware configuration, known issues, and how to restore the original lighting. A similar-looking laptop is not evidence of compatibility. The hardware interface and key mapping still need to match.

[Source and setup](https://github.com/ZavierChen/BetterRGB-ITE8291-Adaptive) · [Architecture](ARCHITECTURE_ZH.md) · [Compatibility matrix](COMPATIBILITY_MATRIX.md)

## Surface Lab: Let students test what an equation will look like

**Question.** Could an interactive graph help students predict how a surface changes, instead of memorizing a separate picture for every equation?

**Design choice.** Put the editable equation, 3D surface, and 2D cross-section in the same view. For the example x² + z² − y² = Q, changing Q through zero lets a reader compare the one-sheet hyperboloid, the cone, and the two-sheet hyperboloid without changing the rest of the equation. During a drag the app renders a simpler preview; it restores detail after the input settles.

**Evidence and limits.** The repository includes regression tests for classification, the transition through Q = 0, cross-sections, and camera behavior. The demo runs in the browser. Finite sampling can miss small features on more complicated surfaces; that limitation is documented.

[Try the English demo](https://zavierchen.github.io/surface-lab/) · [English documentation and tests](https://github.com/ZavierChen/surface-lab/blob/main/README.en.md)

## Bulldog Xiangqi: Decide when a computed result is safe to reuse

**Question.** If a Xiangqi analysis tool has already searched several possible replies, how can it reuse that work without showing advice for the wrong position?

**Design choice.** Use Pikafish for search and scoring. Treat recognition, legal move tracking, and reuse of search results as separate problems. A cached line is reusable only when the confirmed board, move history, and actual reply agree. In the local sandbox, editing the board invalidates the previous suggestions.

**Evidence and limits.** The algorithm notes document the reuse checks, test commands, and fallback behavior. This project uses an existing engine; the scheduling changes are not evidence of stronger play.

**From engineering to research.** An apparent sacrifice that briefly recovered under deeper search showed why a shallow evaluation is not a stopping rule. Background analysis raised a second question: which possible reply is worth computing before the opponent moves? The theory-first proposal studies that question under a fixed budget, with a small model showing how a check can matter because it determines whether a second check is worthwhile. The proposed evaluation separates response time, total computation, and move quality.

[Research proposal](https://github.com/ZavierChen/bulldog-xiangqi/blob/main/docs/RESEARCH_PROPOSAL.md) · [Source and setup](https://github.com/ZavierChen/bulldog-xiangqi) · [Algorithm notes](https://github.com/ZavierChen/bulldog-xiangqi/blob/main/docs/ALGORITHMS.md)
