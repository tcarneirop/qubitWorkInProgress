import sys
from pathlib import Path

import numpy as np
import matplotlib.pyplot as plt


def make_plot(filename, quantity_name, output_file):
    data = np.loadtxt(filename, ndmin=2)

    if data.size == 0:
        print(f"Nenhum dado encontrado em {filename}")
        return

    if data.shape[1] != 2:
        raise ValueError(
            f"{filename}: esperado um arquivo com duas colunas."
        )

    # Coluna 1: valor de Depth ou Swaps
    values = data[:, 0]

    # Coluna 2: quantidade de soluções com aquele valor
    counts = data[:, 1]

    min_value = values.min()
    max_value = values.max()

    if min_value == max_value:
        print(
            f"Não é possível dividir o intervalo de {quantity_name}: "
            "todos os valores são iguais."
        )
        return

    # Dividir o intervalo em 20 faixas de mesma largura
    edges = np.linspace(min_value, max_value, 21)

    # Somar a quantidade de soluções em cada faixa
    bin_counts, _ = np.histogram(
        values,
        bins=edges,
        weights=counts,
    )
    total_solutions = bin_counts.sum()
    bin_percentages = bin_counts / total_solutions * 100

    # Rótulos do eixo X
    labels = [
        f"{i * 5}–{(i + 1) * 5}%"
        for i in range(20)
    ]

    fig, ax = plt.subplots(figsize=(12, 6))

    bars = ax.bar(range(20), bin_percentages)

    #for bar, value in zip(bars, bin_percentages):
    #    if value > 0:
    #        ax.text(
    #        bar.get_x() + bar.get_width() / 2,
    #        bar.get_height(),
    #        f"{value:.3f}%",
    #        ha="center",
    #        va="bottom",
    #        fontsize=8,
    #        rotation=90,
    #    )
    

    ax.set_xticks(range(20))
    ax.set_xticklabels(
        labels,
        rotation=45,
        ha="right",
    )

    ax.set_ylim(0, 23)
    ax.set_yticks(np.arange(0, 26, 5))
    ax.tick_params(axis="x", labelsize=19)
    ax.tick_params(axis="y", labelsize=19)
    ax.set_xlabel(
        f"{quantity_name} interval (% of observed depth range)", fontsize=20
    )
    
    ax.set_ylabel("Solutions (%)", fontsize=20)

   # ax.set_title(
   #     f"Distribution of solutions by {quantity_name} range"
   # )

    ax.grid(axis="y", alpha=0.3)

    fig.savefig(output_file, bbox_inches="tight")
    plt.close(fig)

    print(f"Gráfico salvo em: {output_file}")
    print(f"Total de soluções contabilizadas: {int(bin_counts.sum())}")


if __name__ == "__main__":

    if len(sys.argv) != 3:
        print(
            f"Uso: python3 {sys.argv[0]} "
            "depth_counts.dat swap_counts.dat"
        )
        sys.exit(1)

    depth_file = Path(sys.argv[1])
    swap_file = Path(sys.argv[2])

    make_plot(
        depth_file,
        "Depth",
        depth_file.with_name(
            depth_file.stem + "_20_bins.pdf"
        ),
    )

    make_plot(
        swap_file,
        "Swaps",
        swap_file.with_name(
            swap_file.stem + "_20_bins.pdf"
        ),
    )
