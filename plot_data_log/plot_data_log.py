#!/usr/bin/env python

import click
import matplotlib
import matplotlib.pyplot as plt
import pandas

@click.command()
@click.argument('data_log', type=click.Path(exists=True))
def plot_data_log(data_log):
    matplotlib.use('gtk3agg')

    data = pandas.read_csv(data_log)
    data.plot("Timestamp", ["Phase Error"], figsize=(200, 8))
    # plt.savefig('foo.png', dpi=200, bbox_inches='tight')
    plt.show()

if __name__ == '__main__':
    plot_data_log()