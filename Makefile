.PHONY: build test demo link-sweep ofdm-sweep manet-sweep clean

build:
	docker compose build

test: build
	docker compose run --rm crosslink test

demo: build
	docker compose run --rm crosslink demo

link-sweep: build
	docker compose run --rm crosslink link-sweep

ofdm-sweep: build
	docker compose run --rm crosslink ofdm-sweep

manet-sweep: build
	docker compose run --rm crosslink manet-sweep

clean:
	cmake -E remove_directory build
	cmake -E remove -f results/*.csv results/*.png results/report.html
