# 实验输出

运行 `make demo` 后，本目录包含以下可复现产物：

- `report.html`：总览报告；
- `link-sweep.csv`、`ber-awgn.png`、`ber-rayleigh.png`：调制编码链路；
- `ofdm-sweep.csv`、`ofdm-equalizers.png`：OFDM 接收机；
- `adaptive-mcs.csv`、`adaptive-mcs.png`：目标 BER 链路自适应；
- `manet-sweep.csv`、`routing-comparison.png`：四种 MANET 路由协议对比；
- `mobility-*.csv`、`routes-*.csv`：体积较大的逐时刻轨迹，默认被 Git 忽略。

CSV 与图片均由脚本生成。公开仓库时建议保留汇总 CSV 和 PNG，便于招聘方无需安装环境即可快速查看结果。
