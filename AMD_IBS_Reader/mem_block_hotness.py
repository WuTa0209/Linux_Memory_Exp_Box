#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
mem_block_hotness.py  ─ 0x0000 ~ 0x0FFFFFFF
-----------------------------------------------------
• 4 KiB page (= 2¹²) for one block。
• show the heat graph first，and show the hist bar (Top-N page)。
• support PID filter，only show target PID memory access info。
• support image saving functionality
"""

from __future__ import annotations
import argparse
from collections import Counter
from pathlib import Path
from typing import List, Optional

import numpy as np
import pandas as pd
import matplotlib.pyplot as plt
from matplotlib.ticker import MaxNLocator
from datetime import datetime



def parse_args() -> argparse.Namespace:
    ap = argparse.ArgumentParser(
        description="0x0–0x0FFFFFFFF phys_addr access heatmap + bar chart"
    )
    ap.add_argument("csv", type=Path, help="CSV path")
    ap.add_argument("-d", "--delimiter", default=",", help="CSV delimiter (default ',')")
    ap.add_argument("--header", action="store_true", help="CSV has header row")
    ap.add_argument("--index", "-i", type=int, default=6, help="phys_addr column index (0-based)")
    ap.add_argument("--pid-index", type=int, default=1, help="PID column index (0-based, default 1)")
    ap.add_argument("--pid", type=int, nargs="+", help="Filter by PID(s) - can specify multiple PIDs")
    ap.add_argument("--bar", action="store_true", help="also draw bar chart (Top-N)")
    ap.add_argument("--top", type=int, default=50, help="Top-N pages for bar chart")
    
    ap.add_argument("--save", action="store_true", help="Save plots to files instead of displaying")
    ap.add_argument("--output-dir", type=Path, default=".", help="Output directory for saved plots")
    ap.add_argument("--dpi", type=int, default=300, help="DPI for saved images (default 300)")
    ap.add_argument("--format", choices=["png", "pdf", "svg", "jpg"], default="png", help="Image format")
    
    return ap.parse_args()


MAX_ADDR   = 0xFFFFFFFFFF
PAGE_SHIFT = 12               # 4 KiB
N_ROWS     = 256              # 2⁸
N_COLS     = 256              # 2⁸

def load_addrs(path: Path, phys_addr_col: int, pid_col: int, delim: str, hdr: bool, pids: Optional[List[int]] = None) -> List[int]:
    """
    load physical addresses from CSV, optionally filtering by PID(s)
    """
    print(f"[Debug] load column: phys_addr_col={phys_addr_col}, pid_col={pid_col}")
    
    if pids is not None:
        # read both PID and physical address columns
        df = pd.read_csv(
            path, sep=delim, header=0 if hdr else None,
            usecols=[pid_col, phys_addr_col],
            dtype={pid_col: int, phys_addr_col: str},
            engine="c"
        )

        # rename columns to avoid confusion
        df.columns = ['pid', 'phys_addr']
        
        print(f"[Debug] load {len(df)} records from CSV")
        print(f"[Debug] PID range: {df['pid'].min()} ~ {df['pid'].max()}")
        print(f"[Debug] PIDs to filter: {pids}")

        # filter by PID
        mask = df['pid'].isin(pids)
        df_filtered = df[mask]

        print(f"[Debug] After filtering: {len(df_filtered)} records remaining")

        if df_filtered.empty:
            print(f"[Warning] No data found for PID {pids}")
            return []
        
        print(f"[Info] Found {len(df_filtered)} records for PID {pids}")
        phys_addr_series = df_filtered['phys_addr']

    else:
        df = pd.read_csv(
            path, sep=delim, header=0 if hdr else None,
            usecols=[phys_addr_col], dtype=str, engine="c"
        )
        phys_addr_series = df.iloc[:, 0]
    
    valid_addrs = (
        phys_addr_series
          .str.strip()
          .loc[lambda s: s.str.startswith("0x")]
    )

    print(f"[Debug] valid: {len(valid_addrs)}")

    result = []
    for addr_str in valid_addrs:
        try:
            addr = int(addr_str, 16)
            if addr <= MAX_ADDR:
                result.append(addr)
        except ValueError:
            continue

    print(f"[Debug] final valid: {len(result)}")
    return result


def draw_heatmap(counter: Counter[int], pids: Optional[List[int]] = None, save_path: Optional[Path] = None, dpi: int = 300):
    """draw heatmap, support saving to file"""
    mat = np.zeros((N_ROWS, N_COLS), dtype=int)

    for page, cnt in counter.items():
        r = (page >> 8) & 0xFF
        c =  page        & 0xFF
        mat[r, c] += cnt

    fig, ax = plt.subplots(figsize=(8, 8))
    im = ax.imshow(mat, cmap="Blues", interpolation="nearest", vmin=0)
    
    if pids:
        title = f"Physical Address Heatmap (PID: {', '.join(map(str, pids))})\n4 KiB/page in 0x0–0xFFFFFFFFFF"
    else:
        title = "Physical Address Heatmap\n4 KiB/page in 0x0–0xFFFFFFFFFF"
    
    ax.set_title(title, pad=15, fontsize=12)
    ax.axis("off")
    cbar = fig.colorbar(im, ax=ax, shrink=0.8, label="Access Count")
    cbar.ax.yaxis.set_major_locator(MaxNLocator(integer=True))
    plt.tight_layout()
    
    if save_path:
        plt.savefig(save_path, dpi=dpi, bbox_inches='tight')
        print(f"[Info] Heatmap saved to: {save_path}")
        plt.close()
    else:
        plt.show()


def draw_bar(counter: Counter[int], top_n: int, pids: Optional[List[int]] = None, save_path: Optional[Path] = None, dpi: int = 300):
    """draw bar chart, support saving to file"""
    items = counter.most_common(top_n)
    if not items:
        print("[Warning] No data available to draw bar chart")
        return
        
    labels = [f"0x{page << PAGE_SHIFT:010x}" for page, _ in items]
    counts = [cnt for _, cnt in items]

    fig_width = max(10, 0.4 * len(items))
    fig, ax = plt.subplots(figsize=(fig_width, 8))
    bars = ax.bar(range(len(items)), counts, color="#1f77b4", alpha=0.8)

    # Show values on bars
    for i, (bar, count) in enumerate(zip(bars, counts)):
        ax.text(bar.get_x() + bar.get_width()/2, bar.get_height() + max(counts)*0.01,
                str(count), ha='center', va='bottom', fontsize=8)
    
    ax.set_xticks(range(len(items)))
    ax.set_xticklabels(labels, rotation=45, fontsize=9, ha='right')
    ax.set_ylabel("Access Count", fontsize=11)
    ax.set_xlabel("Physical Address (Page)", fontsize=11)
    
    if pids:
        title = f"Top-{top_n} Hottest Memory Pages (PID: {', '.join(map(str, pids))})\n4 KiB per page"
    else:
        title = f"Top-{top_n} Hottest Memory Pages\n4 KiB per page"
    
    ax.set_title(title, fontsize=12, pad=15)
    ax.yaxis.set_major_locator(MaxNLocator(integer=True))
    ax.grid(axis='y', alpha=0.3)
    plt.tight_layout()
    
    if save_path:
        plt.savefig(save_path, dpi=dpi, bbox_inches='tight')
        print(f"[Info] Bar chart saved to: {save_path}")
        plt.close()
    else:
        plt.show()


def main():
    args = parse_args()
    
    # Create output directory if saving images
    if args.save:
        args.output_dir.mkdir(parents=True, exist_ok=True)

    # Load address data (optionally filter by PID)
    addrs = load_addrs(
        args.csv, 
        args.index, 
        args.pid_index, 
        args.delimiter, 
        args.header,
        args.pid
    )
    
    if not addrs:
        print("[Error] No valid memory address data found")
        return

    print(f"[Info] Total processed {len(addrs)} memory addresses")

    # Convert to page numbers and count accesses
    pages = [addr >> PAGE_SHIFT for addr in addrs]
    cnt = Counter(pages)

    print(f"[Info] Found {len(cnt)} different memory pages")

    if not cnt:
        print("[Error] No memory pages found")
        return

    # Generate filenames (add timestamp)
    if args.save:
        # Create timestamp
        from datetime import datetime
        timestamp = datetime.now().strftime("%Y%m%d_%H%M%S")

        # Generate appropriate filenames based on PID
        if args.pid:
            pid_suffix = f"_pid_{'_'.join(map(str, args.pid))}"
        else:
            pid_suffix = "_all"
        
        heatmap_filename = f"memory_heatmap{pid_suffix}_{timestamp}.{args.format}"
        heatmap_path = args.output_dir / heatmap_filename
        
        bar_filename = f"memory_bar_top{args.top}{pid_suffix}_{timestamp}.{args.format}"
        bar_path = args.output_dir / bar_filename
    else:
        heatmap_path = None
        bar_path = None

    # Draw heatmap
    draw_heatmap(cnt, args.pid, heatmap_path, args.dpi)

    # If requested, draw bar chart
    if args.bar:
        draw_bar(cnt, args.top, args.pid, bar_path, args.dpi)
    
    if args.save:
        print(f"\n[Info] All images saved to directory: {args.output_dir}")

if __name__ == "__main__":
    main()
