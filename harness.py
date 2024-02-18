
import os
import re
import subprocess
import sys

def run_stderr(s):
    r = subprocess.run(s.split(' '), stderr=subprocess.PIPE)
    return r.stderr.decode('utf-8').strip()

def run(s):
    r = subprocess.run(s.split(' '), stderr=subprocess.STDOUT, stdout=subprocess.PIPE)
    return r.stdout.decode('utf-8').strip()

def generate_kryo(rate):
    cfg = f'''
disk ibm.1440
    cyls = 80
    heads = 2
    tracks * ibm.mfm
        secs = 18
        bps = 512
        gap3 = 84
        rate = {rate}
    end
end'''

    with open('out/a.cfg', 'w') as f:
        f.write(cfg)
    
    s = run_stderr('gw convert ibm1440.img out/00.0.raw --diskdefs=out/a.cfg --format=ibm.1440 --tracks=c=0-1')
#    print(s)

def generate_ff(precomp):
    s = run(f'./kryoflux_to_flashfloppy/target/debug/kryoflux_to_flashfloppy out/00.0.raw --out-dir out --write-precomp-ns {precomp}')
#    print(s)

def check_algorithm(integral_div, error_div):
    print(f'Checking I={integral_div} E={error_div}')
    if os.path.isfile('out/a.hfe'):
        os.remove('out/a.hfe')
    s = run(f'./flashfloppy_to_hfe/flashfloppy_to_hfe out/00.0.revolution1.ff_samples out/ 500 nco_v1[p_mul=1,p_div={error_div},i_mul=1,i_div={integral_div}]')
    if not os.path.isfile(f'out/00.0.revolution1.500_nco_v1[p_mul=1,p_div={error_div},i_mul=1,i_div={integral_div}].hfe'):
        return False
    s = run(f'gw convert out/00.0.revolution1.500_nco_v1[p_mul=1,p_div={error_div},i_mul=1,i_div={integral_div}].hfe out/a.img --format=ibm.1440 --tracks=c=0:h=0')
    m = re.search(r'Found (\d+) sectors of 18', s)
    return int(m.group(1)) == 18

def main(argv):

    # Establish baseline
    successes = []
    generate_kryo(500)
    generate_ff(0)
    for integral_div in range(4,16):
        for error_div in range(2,16):
            if check_algorithm(1<<integral_div, 1<<error_div):
                successes.append((integral_div,error_div))
    print(successes)
    print()

    final = []
    for rate in range(460,540+1, 5):
        for precomp in range(0, 400, 50):
            print(f'Generating {rate} @{precomp}')
            generate_kryo(rate)
            generate_ff(precomp)
            for integral_div, error_div in successes:
                if check_algorithm(1<<integral_div, 1<<error_div):
                    final.append((rate,precomp,integral_div,error_div))
    print(final)
    with open('results', 'w') as f:
        for x in final:
            f.write(str(x) + '\n')

if __name__ == "__main__":
    main(sys.argv)

# Local variables:
# python-indent: 4
# End:
