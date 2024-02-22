#!/usr/bin/env python3
import csv
import itertools
import os
import re
import subprocess
import sys
import typing
from dataclasses import dataclass
from multiprocessing import Pool

@dataclass
class Format:
    name: str
    heads: int
    cylinders: int
    sectors_per_cylinder: int
    bytes_per_sector: int
    data_rate_kbps: int
    rpm: int

    def sectors(self):
        return self.heads * self.cylinders * self.sectors_per_cylinder

FORMATS=[
    Format(
        'ibm360',
        heads=2,
        cylinders=40,
        sectors_per_cylinder=9,
        bytes_per_sector=512,
        data_rate_kbps=250,
        rpm=300
    ),
    Format(
        'ibm1440',
        heads=2,
        cylinders=80,
        sectors_per_cylinder=18,
        bytes_per_sector=512,
        data_rate_kbps=500,
        rpm=300
    ),
    Format(
        'ibm.ed',
        heads=2,
        cylinders=80,
        sectors_per_cylinder=18,
        bytes_per_sector=512,
        data_rate_kbps=1000,
        rpm=600
    )
]

PRECOMP_MIN=0
PRECOMP_MAX=400

@dataclass
class Algorithm:
    name: str
    p_div_range: range
    i_div_range: range
    name_format: str

ALGORITHMS = [
    Algorithm(
        name='flashfloppy_master',
        p_div_range = range(1),
        i_div_range = range(1),
        name_format = 'flashfloppy_master'
    ),
    Algorithm(
        name='greaseweazle_default_pll',
        p_div_range = range(1),
        i_div_range = range(1),
        name_format = 'greaseweazle_default_pll'
    ),
    Algorithm(
        name='nco_v1',
        p_div_range = range(2, 18),
        i_div_range = range(4, 20),
        name_format = 'nco_v1[p_mul=1,p_div={p_div},i_mul=1,i_div={i_div}]'
    ),
    Algorithm(
        name='nco_v2',
        p_div_range = range(2, 18),
        i_div_range = range(4, 20),
        name_format = 'nco_v2[p_mul=1,p_div={p_div},i_mul=1,i_div={i_div}]'
    ),
    Algorithm(
        name='nco_v3',
        p_div_range = range(2, 18),
        i_div_range = range(4, 20),
        name_format = 'nco_v3[p_mul=1,p_div={p_div},i_mul=1,i_div={i_div}]'
    )
]

def run_stderr(s):
    r = subprocess.run(s.split(' '), stderr=subprocess.PIPE)
    return r.stderr.decode('utf-8').strip()


def run(s):
    r = subprocess.run(s.split(' '), stderr=subprocess.STDOUT, stdout=subprocess.PIPE)
    return r.stdout.decode('utf-8').strip()


def generate_diskdef(format, rate, out_dir):
    cfg_filename = f'{out_dir}/{format.name}.{rate}.cfg'
    if os.path.isfile(cfg_filename):
        return cfg_filename

    cfg = f'''
disk {format.name}.{rate}
    cyls = {format.cylinders}
    heads = {format.heads}
    tracks * ibm.mfm
        secs = {format.sectors_per_cylinder}
        bps = {format.bytes_per_sector}
        gap3 = 84
        rate = {rate}
        rpm = {format.rpm}
    end
end'''
    with open(cfg_filename, 'w') as f:
        f.write(cfg)

    return cfg_filename


def generate_kryo(format, rate, out_dir):
    img_filename = f'{out_dir}/{format.name}.img'
    if not os.path.isfile(img_filename):
        run_stderr(f'dd if=/dev/random of={img_filename} bs={format.bytes_per_sector} count={format.sectors()}')

    cfg_filename = generate_diskdef(format, rate, out_dir)
    s = run_stderr(
        f'gw convert {img_filename} {out_dir}/00.0.raw --diskdefs={cfg_filename} --format={format.name}.{rate} --tracks=c=0:h=0'
    )


#    print(s)


def generate_ff(precomp, out_dir):
    s = run(
        f'../kryoflux_to_flashfloppy/target/debug/kryoflux_to_flashfloppy {out_dir}/00.0.raw --out-dir {out_dir} --write-precomp-ns {precomp}'
    )


#    print(s)


def check_algorithm(format, algorithm, proportial_div, integral_div, out_dir):
    algorithm_name = algorithm.name_format.format(p_div=proportial_div, i_div=integral_div)
    out_filename = f'{out_dir}/00.0.revolution1.{format.data_rate_kbps}_{algorithm_name}.hfe'
    img_filename = f'{out_dir}/00.0.revolution1.{format.data_rate_kbps}_{algorithm_name}.img'

    print(f'Checking {algorithm_name}...', end='')

    if os.path.isfile(out_filename):
        os.remove(out_filename)

    s = run(
        f'../flashfloppy_to_hfe/flashfloppy_to_hfe {out_dir}/00.0.revolution1.ff_samples {out_dir}/ {format.data_rate_kbps} {algorithm_name}'
    )
    if not os.path.isfile(out_filename):
        print('fail')
        return (proportial_div, integral_div, False)
    
    cfg_filename = generate_diskdef(format, format.data_rate_kbps, out_dir)
    s = run(
        f'gw convert {out_filename} {img_filename} --diskdefs={cfg_filename} --format={format.name}.{format.data_rate_kbps} --tracks=c=0:h=0'
    )
    m = re.search(r'Found (\d+) sectors of \d+', s)
    if int(m.group(1)) == format.sectors_per_cylinder:
        print('pass')
        return (proportial_div, integral_div, True)
    else:
        print('fail')
        return (proportial_div, integral_div, False)


def main(argv):
    out_dir = f'out'

    if not os.path.isdir(out_dir):
        os.makedirs(out_dir)

    with open(f'{out_dir}/results.csv', 'w', newline='', buffering=1) as f:
        resultwriter = csv.writer(f)
        resultwriter.writerow(['Rate (kbps)', 'Precomp (ns)', 'Algorithm', 'p_div', 'i_div'])

        with Pool(48) as pool:
            for format in FORMATS:
                data_rate_min = round(format.data_rate_kbps*.92/5) * 5
                data_rate_max = round(format.data_rate_kbps*1.08/5) * 5
                data_rate_step = max(round((data_rate_max-data_rate_min)/16/5) * 5, 5)

                for rate in range(data_rate_min, data_rate_max + 1, data_rate_step):
                    generate_kryo(format, rate, out_dir)
                    for precomp in range(PRECOMP_MIN, PRECOMP_MAX + 1, 50):
                        print(f'Generating {rate} @{precomp}')
                        generate_ff(precomp, out_dir)
                        for algorithm in ALGORITHMS:
                            for (p_div, i_div, result) in pool.starmap(check_algorithm, [(format, algorithm, (1 << i_div), (1 << p_div), out_dir) for (i_div, p_div) in itertools.product(algorithm.p_div_range, algorithm.i_div_range)]):
                                if result:
                                    resultwriter.writerow([rate, precomp, algorithm.name, p_div, i_div])

if __name__ == '__main__':
    main(sys.argv)

# Local variables:
# python-indent: 4
# End:
