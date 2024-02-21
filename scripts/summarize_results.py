#!/usr/bin/env python3

import click
import pandas as pd


@click.command()
@click.argument("result_file", type=click.File("r"))
def summarize_results(result_file):
    results = pd.read_csv(result_file)

    for algorithm in results["Algorithm"].unique():
        alg_data = results[results["Algorithm"] == algorithm]
        print(f"{algorithm}: Count of successful parameter combinations")
        print('=' * 60)

        print(
            alg_data.groupby(["Rate (kbps)", "Precomp (ns)"], as_index=False)
            .size()
            .pivot_table(
                index="Rate (kbps)", columns="Precomp (ns)", values="size", fill_value=0
            )
            .astype(int)
            .style.format("{:>5}")
            .format_index("{:>12}", axis=0)
            .format_index("{:>5}", axis=1)
            .to_string()
        )

        print('Parameters with largest coverage:')
        group_by_coverage = alg_data.groupby(["p_div", "i_div"]).size()
        for (p_div, i_div) in group_by_coverage[group_by_coverage == group_by_coverage.max()].index.to_list():
            print(f'  p_mul=1, p_div={p_div}, i_mul=1, i_div={i_div}')
        print('=' * 60)

        [(p_div, i_div)] = (
            alg_data.groupby(["p_div", "i_div"]).size().nlargest(1).index.to_list()
        )
        print(
            alg_data.loc[(alg_data['p_div'] == p_div) & (alg_data['i_div'] == i_div)]
            .groupby(['Rate (kbps)', 'Precomp (ns)'], as_index=False)
            .size()
            .pivot_table(index='Rate (kbps)', columns='Precomp (ns)', values='size', fill_value=0)
            .astype(int)
            .style.format("{:>5}")
            .format_index("{:>12}", axis=0)
            .format_index("{:>5}", axis=1)
            .to_string()
        )


if __name__ == "__main__":
    summarize_results()
