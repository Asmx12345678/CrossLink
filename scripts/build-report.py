#!/usr/bin/env python3
import csv
import html
from collections import defaultdict
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
RESULTS = ROOT / "results"


def rows(name: str):
    with (RESULTS / name).open(newline="") as stream:
        return list(csv.DictReader(stream))


def table(data, fields):
    head = "".join(f"<th>{html.escape(label)}</th>" for _, label in fields)
    body = "".join(
        "<tr>" + "".join(f"<td>{html.escape(str(row[key]))}</td>" for key, _ in fields) + "</tr>"
        for row in data
    )
    return f"<table><thead><tr>{head}</tr></thead><tbody>{body}</tbody></table>"


def main():
    manet = rows("manet-sweep.csv")
    grouped = defaultdict(list)
    for row in manet:
        grouped[row["protocol"]].append(row)
    summary = []
    for protocol, values in sorted(grouped.items()):
        summary.append({
            "protocol": protocol,
            "pdr": f'{sum(float(v["pdr_percent"]) for v in values) / len(values):.2f}%',
            "throughput": f'{sum(float(v["throughput_mbps"]) for v in values) / len(values):.3f}',
            "delay": f'{sum(float(v["mean_delay_ms"]) for v in values) / len(values):.3f}',
        })

    content = f"""<!doctype html>
<html lang="zh-CN"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width">
<title>CrossLink 实验报告</title><style>
:root {{ color-scheme: light; --ink:#172033; --muted:#64748b; --accent:#2563eb; }}
* {{ box-sizing:border-box }} body {{ margin:0;background:#f4f7fb;color:var(--ink);font:15px/1.6 system-ui,sans-serif }}
main {{ max-width:1100px;margin:auto;padding:40px 24px }} h1 {{ margin-bottom:4px }} .lead {{ color:var(--muted);margin-top:0 }}
.grid {{ display:grid;grid-template-columns:repeat(auto-fit,minmax(300px,1fr));gap:18px }}
.card {{ background:white;border:1px solid #dce4ef;border-radius:14px;padding:20px;box-shadow:0 8px 24px #1720330a }}
img {{ width:100%;border-radius:8px }} table {{ width:100%;border-collapse:collapse }} th,td {{ padding:9px;border-bottom:1px solid #e5eaf1;text-align:right }} th:first-child,td:first-child {{ text-align:left }}
code {{ background:#e8eef8;padding:2px 5px;border-radius:4px }} footer {{ color:var(--muted);margin-top:24px }}
</style></head><body><main>
<h1>CrossLink 实验报告</h1><p class="lead">无线自组网跨层通信仿真与智能路由优化平台 · 可复现实验输出</p>
<section class="card"><h2>路由协议三随机种子均值</h2>{table(summary, [("protocol","协议"),("pdr","PDR"),("throughput","吞吐量 Mbit/s"),("delay","时延 ms")])}</section>
<div class="grid">
<section class="card"><h2>MANET 路由对比</h2><img src="routing-comparison.png" alt="routing comparison"></section>
<section class="card"><h2>OFDM 接收机</h2><img src="ofdm-equalizers.png" alt="OFDM comparison"></section>
<section class="card"><h2>AWGN 链路</h2><img src="ber-awgn.png" alt="AWGN BER"></section>
<section class="card"><h2>自适应 MCS</h2><img src="adaptive-mcs.png" alt="adaptive MCS"></section>
</div><footer>运行 <code>make demo</code> 可重新生成全部数据、图表与本报告。</footer>
</main></body></html>"""
    (RESULTS / "report.html").write_text(content, encoding="utf-8")
    print(f"report written to {RESULTS / 'report.html'}")


if __name__ == "__main__":
    main()
