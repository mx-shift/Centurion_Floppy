use anyhow::{anyhow, bail, Result};
use byteorder::{LittleEndian, WriteBytesExt};
use clap::Parser;
use vcd::{Writer, Value, TimescaleUnit, SimulationCommand};
use std::{collections::VecDeque, ffi::OsString, path::PathBuf};

    const KRYOFLUX_MCLK_HZ : f64 = ((18_432_000.0 * 73.0) / 14.0) / 2.0;
    const KRYOFLUX_SCLK_HZ : f64 = KRYOFLUX_MCLK_HZ / 2.0;
    //const KRYOFLUX_ICLK_HZ : f64 = KRYOFLUX_MCLK_HZ / 16.0;

    //const FLASHFLOPPY_TICK_HZ : usize = 72_000_000;

/*
    x SCLK -> y TICK

    x SCLK * (1 second / KRYOFLUX_SCLK_HZ SCLK) * (FLASHFLOPPY_TICK_HZ TICK / 1 second)
    x SCLK * (1 second / (KRYOFLUX_MCLK_HZ / 2) SCLK) * (FLASHFLOPPY_TICK_HZ TICK / 1 second)
    x SCLK * (1 second / ( (((18_432_000 * 73) / 14) / 2) / 2) SCLK) * (FLASHFLOPPY_TICK_HZ TICK / 1 second)
    x SCLK * (1 second / ( (((18_432_000 * 73) / 14) / 2) / 2) SCLK) * (72_000_000 TICK / 1 second)
    x SCLK * (72_000_000 TICK / ( (((18_432_000 * 73) / 14) / 2) / 2) SCLK )
    x SCLK * (72_000_000 * 56) / (18_432_000 * 73)
    x SCLK * (72_000 * 56) / (18_432 * 73)
    x SCLK * (72_000 * 7) / (2_304 * 73)
    x SCLK * (1_125 * 7) / (36 * 73)
    x SCLK * (375 * 7) / (12 * 73)
    x SCLK * (125 * 7) / (4 * 73)
*/

#[derive(Parser)]
struct Args {
    infile: PathBuf,

    #[clap(long)]
    out_dir: Option<PathBuf>,

    #[clap(long, default_value_t = 0.0)]
    write_precomp_ns: f64,

    #[clap(long)]
    log: bool,

    #[clap(long)]
    vcd: bool,
}

fn main() -> Result<()> {
    let opts = Args::parse();

    let kryoflux_revs = read_kryoflux_samples(&opts.infile)?;

    // Precomp
    let write_precomp = (opts.write_precomp_ns / 1.0e9 * KRYOFLUX_SCLK_HZ).round() as u16;
    println!("Write precomp: {:.3}ns ({})", opts.write_precomp_ns, write_precomp); 

    let kryoflux_revs = kryoflux_revs.iter().map(|pulse_times| {
        let mut adj_pulse_times = pulse_times.clone();
        let mut history = [0; 2];

        for (idx, interval) in pulse_times.iter().enumerate() {
            let sample_us = (*interval as f64) / KRYOFLUX_SCLK_HZ * 1_000_000.0;
            let sample_us = sample_us.round() as usize;

            history[0] = history[1];
            history[1] = sample_us;

            match history {
                [2, x] if x >= 3 => {
                    adj_pulse_times[idx-1] -= write_precomp;
                    adj_pulse_times[idx] += write_precomp;
                },
                [x, 2] if x >= 3 => {
                    adj_pulse_times[idx-1] += write_precomp;
                    adj_pulse_times[idx] -= write_precomp;
                },
                _ => {}
            }
        }

        adj_pulse_times
    }).collect::<Vec<_>>();

    let outdir = opts.out_dir.unwrap_or_default();

    let outstem = opts.infile.file_stem().ok_or(anyhow!(
            "Failed to file_stem from input file: {}",
            opts.infile.display()
        ))?; 

    for (rev, pulse_times) in kryoflux_revs.into_iter().enumerate() {
        if opts.log {
            let mut log_filename = OsString::new();
            log_filename.push(&outstem);
            log_filename.push(format!(".revolution{}.log", rev));

            let mut log_path = outdir.clone();
            log_path.push(log_filename);

            println!("Writing log to {}", log_path.display());

            let mut log_file = std::fs::File::create(log_path)?;

            write_log_samples(&mut log_file, &pulse_times)?;
        }

        let mut ff_filename = OsString::new();
        ff_filename.push(&outstem);
        ff_filename.push(format!(".revolution{}.ff_samples", rev));

        let mut ff_path = outdir.clone();
        ff_path.push(ff_filename);

        println!("Writing FlashFloppy samples to {}", ff_path.display());

        let mut ff_file = std::fs::File::create(ff_path)?;

        write_ff_samples(&mut ff_file, &pulse_times)?;

        if opts.vcd {
            let mut vcd_filename = OsString::new();
            vcd_filename.push(&outstem);
            vcd_filename.push(format!(".revolution{}.vcd", rev));

            let mut vcd_path = outdir.clone();
            vcd_path.push(vcd_filename);

            println!("Writing VCD to {}", vcd_path.display());

            let mut vcd_file = std::fs::File::create(vcd_path)?;

            write_vcd(&mut vcd_file, &pulse_times)?;
        }
    }

    Ok(())
}

fn read_kryoflux_samples(
    kryoflux_raw_path: &PathBuf,
) -> Result<Vec<Vec<u16>>> {
    let mut infile: VecDeque<u8> = std::fs::read(&kryoflux_raw_path)?.into();

    let mut result : Vec<Vec<u16>> = Vec::new();
    let mut cur_pulse_intervals = Vec::<u16>::new();

    while let Some(header) = infile.pop_front() {
        match header {
            0x00..=0x07 /* Flux2 */ => {
                let lower = infile.pop_front().ok_or(anyhow!("EOF during Flux2"))?;

                cur_pulse_intervals.push(((header as u16) << 8) + lower as u16);
            }
            0x08 /* Nop1 */=> {}
            0x09 /* Nop2 */=> {
                infile.pop_front().ok_or(anyhow!("EOF during NOP2"))?;
            }
            0x0A /* Nop3 */=> {
                infile.pop_front().ok_or(anyhow!("EOF during NOP3"))?;
                infile.pop_front().ok_or(anyhow!("EOF during NOP3"))?;
            },
            0x0B /* Ovl16 */ => {
                todo!("OVL16")
            },
            0x0C /* Flux3 */=> {
                let upper = infile.pop_front().ok_or(anyhow!("EOF during Flux3"))?;
                let lower = infile.pop_front().ok_or(anyhow!("EOF during Flux3"))?;
                cur_pulse_intervals.push(((upper as u16) << 8) + lower as u16);
            },
            0x0D /* OOB */=> {
                let oob_type = infile.pop_front().ok_or(anyhow!("EOF during OOB"))?;
                let oob_size_lower = infile.pop_front().ok_or(anyhow!("EOF during OOB"))?;
                let oob_size_upper = infile.pop_front().ok_or(anyhow!("EOF during OOB"))?;

                let oob_size = ((oob_size_upper as u16) << 8) + (oob_size_lower as u16);

                match oob_type {
                    0x1 /* StreamInfo */
                    | 0x4 /* KFInfo */ => {
                        for _ in 0..oob_size {
                            infile.pop_front().ok_or(anyhow!("EOF during OOB"))?;  
                        }
                    },
                    0x2 /* Index */ => {
                        for _ in 0..oob_size {
                            infile.pop_front().ok_or(anyhow!("EOF during OOB"))?;  
                        }

                        result.push(cur_pulse_intervals.clone());
                        cur_pulse_intervals.clear();
                    }
                    0x3 /* StreamEnd */ => break,
                    0x0 | _ => bail!("Invalid OOB"),
                }
            }
            0x0E..=0xFF /* Flux1 */ => cur_pulse_intervals.push(header as u16),
        }
    }

    result.push(cur_pulse_intervals);
    Ok(result)
}

fn write_ff_samples<W>(
    outfile: &mut W,
    pulse_intervals: &[u16],
) -> Result<()>
where
    W: std::io::Write
{
    let mut tick_counter: u16 = 0x4321;

    for interval in pulse_intervals {
        let sample_ff_ticks = (*interval as usize) * (125 * 7) / (4 * 73);

        (tick_counter, _) = tick_counter.overflowing_add(sample_ff_ticks as u16);

        outfile.write_u16::<LittleEndian>(tick_counter)?;
    }

    Ok(())
}

fn write_vcd<W>(
    outfile: &mut W,
    pulse_intervals: &[u16],
) -> Result<()>
where
    W: std::io::Write
{
    let mut writer = Writer::new(outfile);

    writer.timescale(41_619, TimescaleUnit::PS)?;
    writer.add_module("top")?;
    let flux = writer.add_wire(1, "flux")?;
    writer.upscope()?;
    writer.enddefinitions()?;

    writer.begin(SimulationCommand::Dumpvars)?;
    writer.change_scalar(flux, Value::V0)?;
    writer.end()?;

    let mut t = 0;
    for interval in pulse_intervals {
        // Pulses are roughly ~250ns long
	    let pulse_start = (*interval as u64).saturating_sub(6);
        writer.timestamp(t + pulse_start)?;
        writer.change_scalar(flux, Value::V1)?;

        t += *interval as u64;
        writer.timestamp(t)?;
        writer.change_scalar(flux, Value::V0)?;
    }
    Ok(())
}

fn write_log_samples<W>(
    outfile: &mut W,
    pulse_intervals: &[u16],
) -> Result<()>
where
    W: std::io::Write
{
    let mut clock_us: f64 = 0.0;
    let mut clock_ff_ticks: u16 = 0;

    let mut sample_us_dev_min: f64 = 0.0;
    let mut sample_us_dev_max: f64 = 0.0;

    for (ii, interval) in pulse_intervals.iter().enumerate() {
        let sample_us = (*interval as f64) / KRYOFLUX_SCLK_HZ * 1_000_000.0;
        clock_us += sample_us;

        let sample_ff_ticks = (*interval as usize) * (125 * 7) / (4 * 73);
        (clock_ff_ticks, _) = clock_ff_ticks.overflowing_add(sample_ff_ticks as u16);

        let sample_us_nom = sample_us.round();
        let sample_us_dev = sample_us - sample_us_nom;

        if ii > 0 {
            if sample_us_dev < sample_us_dev_min {
                sample_us_dev_min = sample_us_dev;
            }

            if sample_us_dev > sample_us_dev_max {
                sample_us_dev_max = sample_us_dev;
            }
        }

        write!(outfile, "Time: {clock_us:20.9}  From prev: {sample_us:20.9} FF Counter: {clock_ff_ticks} FF interval: {sample_ff_ticks} Nom: {:3} Deviation from nominal: {:3.6}\n", sample_us_nom, sample_us_dev)?;
    }

    write!(outfile, "Sample dev min: {sample_us_dev_min:3.6}, max: {sample_us_dev_max:3.6}\n")?;
    println!("Sample dev min: {sample_us_dev_min:3.6}, max: {sample_us_dev_max:3.6}");

    Ok(())
}
