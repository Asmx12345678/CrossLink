#include "ns3/aodv-module.h"
#include "ns3/applications-module.h"
#include "ns3/core-module.h"
#include "ns3/dsdv-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/network-module.h"
#include "ns3/olsr-module.h"
#include "ns3/wifi-module.h"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <limits>
#include <queue>
#include <sstream>
#include <string>
#include <vector>

using namespace ns3;

namespace {

struct LqsrController {
    NodeContainer nodes;
    Ipv4InterfaceContainer interfaces;
    double radioRange{250.0};
    double interval{2.0};
    std::string routeLog;
    uint32_t updates{0};

    double LinkCost(uint32_t from, uint32_t to) const {
        const auto a = nodes.Get(from)->GetObject<MobilityModel>();
        const auto b = nodes.Get(to)->GetObject<MobilityModel>();
        const Vector delta = b->GetPosition() - a->GetPosition();
        const Vector relativeVelocity = b->GetVelocity() - a->GetVelocity();
        const double distance = std::max(1.0, CalculateDistance(a->GetPosition(), b->GetPosition()));
        if (distance > radioRange) return std::numeric_limits<double>::infinity();

        const double pathLossDb = 40.0 + 20.0 * std::log10(distance);
        const double snrDb = 16.0 - pathLossDb - (-95.0);
        const double packetError = 1.0 / (1.0 + std::exp((snrDb - 10.0) / 2.0));
        const double etx = 1.0 / std::max(0.02, 1.0 - packetError);
        const double separationRate =
            (delta.x * relativeVelocity.x + delta.y * relativeVelocity.y) / distance;
        const double mobilityPenalty = 0.06 * std::max(0.0, separationRate);
        const double rangePenalty = 0.5 * distance / radioRange;
        return etx + mobilityPenalty + rangePenalty;
    }

    std::vector<uint32_t> BestPath(uint32_t source, uint32_t destination) const {
        const uint32_t count = nodes.GetN();
        std::vector<double> distance(count, std::numeric_limits<double>::infinity());
        std::vector<int32_t> previous(count, -1);
        using State = std::pair<double, uint32_t>;
        std::priority_queue<State, std::vector<State>, std::greater<State>> queue;
        distance[source] = 0.0;
        queue.push({0.0, source});

        while (!queue.empty()) {
            const auto [cost, node] = queue.top();
            queue.pop();
            if (cost != distance[node]) continue;
            if (node == destination) break;
            for (uint32_t neighbor = 0; neighbor < count; ++neighbor) {
                if (neighbor == node) continue;
                const double edge = LinkCost(node, neighbor);
                if (!std::isfinite(edge)) continue;
                const double candidate = cost + edge;
                if (candidate < distance[neighbor]) {
                    distance[neighbor] = candidate;
                    previous[neighbor] = static_cast<int32_t>(node);
                    queue.push({candidate, neighbor});
                }
            }
        }

        if (!std::isfinite(distance[destination])) return {};
        std::vector<uint32_t> path;
        for (int32_t at = static_cast<int32_t>(destination); at >= 0;
             at = previous[static_cast<uint32_t>(at)]) {
            path.push_back(static_cast<uint32_t>(at));
            if (static_cast<uint32_t>(at) == source) break;
        }
        if (path.back() != source) return {};
        std::reverse(path.begin(), path.end());
        return path;
    }

    void RemoveHostRoute(Ptr<Ipv4StaticRouting> routing, Ipv4Address destination) const {
        for (int64_t index = static_cast<int64_t>(routing->GetNRoutes()) - 1; index >= 0; --index) {
            const auto entry = routing->GetRoute(static_cast<uint32_t>(index));
            if (entry.IsHost() && entry.GetDest() == destination) {
                routing->RemoveRoute(static_cast<uint32_t>(index));
            }
        }
    }

    void InstallDirection(const std::vector<uint32_t>& path) const {
        if (path.size() < 2U) return;
        Ipv4StaticRoutingHelper helper;
        const Ipv4Address destination = interfaces.GetAddress(path.back());
        for (std::size_t i = 0; i + 1U < path.size(); ++i) {
            auto ipv4 = nodes.Get(path[i])->GetObject<Ipv4>();
            auto routing = helper.GetStaticRouting(ipv4);
            RemoveHostRoute(routing, destination);
            routing->AddHostRouteTo(
                destination, interfaces.GetAddress(path[i + 1U]), 1, 1);
        }
    }

    void Update() {
        ++updates;
        const uint32_t source = 0U;
        const uint32_t destination = nodes.GetN() - 1U;
        const auto forward = BestPath(source, destination);
        auto reverse = forward;
        std::reverse(reverse.begin(), reverse.end());
        InstallDirection(forward);
        InstallDirection(reverse);

        std::ofstream out(routeLog, std::ios::app);
        out << std::fixed << std::setprecision(3) << Simulator::Now().GetSeconds() << ',';
        if (forward.empty()) {
            out << "unreachable\n";
        } else {
            for (std::size_t i = 0; i < forward.size(); ++i) {
                if (i) out << '-';
                out << forward[i];
            }
            out << '\n';
        }
        Simulator::Schedule(Seconds(interval), &LqsrController::Update, this);
    }
};

void WriteMobilityHeader(const std::string& path) {
    std::ofstream out(path);
    out << "time_s,node,x_m,y_m,speed_mps\n";
}

void LogMobility(NodeContainer nodes, const std::string& path, double interval) {
    std::ofstream out(path, std::ios::app);
    out << std::fixed << std::setprecision(3);
    for (uint32_t i = 0; i < nodes.GetN(); ++i) {
        const auto mobility = nodes.Get(i)->GetObject<MobilityModel>();
        const auto position = mobility->GetPosition();
        const auto velocity = mobility->GetVelocity();
        out << Simulator::Now().GetSeconds() << ',' << i << ','
            << position.x << ',' << position.y << ','
            << std::sqrt(velocity.x * velocity.x + velocity.y * velocity.y) << '\n';
    }
    Simulator::Schedule(Seconds(interval), &LogMobility, nodes, path, interval);
}

}  // namespace

int main(int argc, char* argv[]) {
    std::string protocol = "AODV";
    std::string output = "results/manet.csv";
    std::string mobilityOutput = "results/mobility.csv";
    std::string routeOutput = "results/routes.csv";
    uint32_t nodeCount = 30;
    uint32_t seed = 42;
    double duration = 60.0;
    double maxSpeed = 12.0;
    double area = 700.0;
    double txPower = 16.0;
    uint32_t packetSize = 512;
    std::string dataRate = "256kbps";

    CommandLine cmd(__FILE__);
    cmd.AddValue("protocol", "AODV, OLSR, DSDV, or LQSR", protocol);
    cmd.AddValue("nodes", "Number of mobile nodes", nodeCount);
    cmd.AddValue("duration", "Simulation duration in seconds", duration);
    cmd.AddValue("speed", "Maximum node speed in m/s", maxSpeed);
    cmd.AddValue("area", "Square scenario width in meters", area);
    cmd.AddValue("txPower", "Wi-Fi transmit power in dBm", txPower);
    cmd.AddValue("packetSize", "UDP payload bytes", packetSize);
    cmd.AddValue("dataRate", "Per-flow application data rate", dataRate);
    cmd.AddValue("seed", "ns-3 random seed", seed);
    cmd.AddValue("output", "Summary CSV output", output);
    cmd.AddValue("mobilityOutput", "Mobility trace CSV", mobilityOutput);
    cmd.AddValue("routeOutput", "LQSR route trace CSV", routeOutput);
    cmd.Parse(argc, argv);

    NS_ABORT_MSG_IF(nodeCount < 4U, "at least four nodes are required");
    SeedManager::SetSeed(seed);
    SeedManager::SetRun(1);

    NodeContainer nodes;
    nodes.Create(nodeCount);

    YansWifiChannelHelper channel;
    channel.SetPropagationDelay("ns3::ConstantSpeedPropagationDelayModel");
    channel.AddPropagationLoss(
        "ns3::LogDistancePropagationLossModel",
        "Exponent", DoubleValue(2.2),
        "ReferenceLoss", DoubleValue(40.0));
    channel.AddPropagationLoss(
        "ns3::RangePropagationLossModel",
        "MaxRange", DoubleValue(250.0));
    YansWifiPhyHelper phy;
    phy.SetChannel(channel.Create());
    phy.Set("TxPowerStart", DoubleValue(txPower));
    phy.Set("TxPowerEnd", DoubleValue(txPower));

    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211g);
    wifi.SetRemoteStationManager(
        "ns3::ConstantRateWifiManager",
        "DataMode", StringValue("ErpOfdmRate12Mbps"),
        "ControlMode", StringValue("ErpOfdmRate6Mbps"));
    WifiMacHelper mac;
    mac.SetType("ns3::AdhocWifiMac");
    const NetDeviceContainer devices = wifi.Install(phy, mac, nodes);

    ObjectFactory positionFactory;
    positionFactory.SetTypeId("ns3::RandomRectanglePositionAllocator");
    positionFactory.Set(
        "X", StringValue("ns3::UniformRandomVariable[Min=0.0|Max=" + std::to_string(area) + "]"));
    positionFactory.Set(
        "Y", StringValue("ns3::UniformRandomVariable[Min=0.0|Max=" + std::to_string(area) + "]"));
    Ptr<PositionAllocator> positionAllocator = positionFactory.Create<PositionAllocator>();

    MobilityHelper mobility;
    mobility.SetPositionAllocator(positionAllocator);
    mobility.SetMobilityModel(
        "ns3::RandomWaypointMobilityModel",
        "Speed", StringValue("ns3::UniformRandomVariable[Min=1.0|Max=" + std::to_string(maxSpeed) + "]"),
        "Pause", StringValue("ns3::ConstantRandomVariable[Constant=0.5]"),
        "PositionAllocator", PointerValue(positionAllocator));
    mobility.Install(nodes);

    InternetStackHelper stack;
    if (protocol == "AODV") {
        AodvHelper helper;
        stack.SetRoutingHelper(helper);
    } else if (protocol == "OLSR") {
        OlsrHelper helper;
        stack.SetRoutingHelper(helper);
    } else if (protocol == "DSDV") {
        DsdvHelper helper;
        stack.SetRoutingHelper(helper);
    } else if (protocol == "LQSR") {
        Ipv4StaticRoutingHelper helper;
        stack.SetRoutingHelper(helper);
    } else {
        NS_FATAL_ERROR("unknown protocol: " << protocol);
    }
    stack.Install(nodes);

    Ipv4AddressHelper address;
    address.SetBase("10.42.0.0", "255.255.0.0");
    const Ipv4InterfaceContainer interfaces = address.Assign(devices);

    constexpr uint16_t port = 9000;
    PacketSinkHelper sink("ns3::UdpSocketFactory",
                          InetSocketAddress(Ipv4Address::GetAny(), port));
    ApplicationContainer sinkApp = sink.Install(nodes.Get(nodeCount - 1U));
    sinkApp.Start(Seconds(0.5));
    sinkApp.Stop(Seconds(duration));

    OnOffHelper source("ns3::UdpSocketFactory",
                       InetSocketAddress(interfaces.GetAddress(nodeCount - 1U), port));
    source.SetAttribute("PacketSize", UintegerValue(packetSize));
    source.SetAttribute("DataRate", DataRateValue(DataRate(dataRate)));
    source.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
    source.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
    ApplicationContainer sourceApp = source.Install(nodes.Get(0));
    sourceApp.Start(Seconds(2.0));
    sourceApp.Stop(Seconds(duration - 0.5));

    LqsrController controller;
    if (protocol == "LQSR") {
        std::ofstream(routeOutput) << "time_s,path\n";
        controller.nodes = nodes;
        controller.interfaces = interfaces;
        controller.routeLog = routeOutput;
        Simulator::Schedule(Seconds(0.8), &LqsrController::Update, &controller);
    }

    WriteMobilityHeader(mobilityOutput);
    Simulator::Schedule(Seconds(0.0), &LogMobility, nodes, mobilityOutput, 1.0);

    FlowMonitorHelper flowHelper;
    Ptr<FlowMonitor> monitor = flowHelper.InstallAll();
    Simulator::Stop(Seconds(duration));
    Simulator::Run();

    monitor->CheckForLostPackets();
    const auto classifier = DynamicCast<Ipv4FlowClassifier>(flowHelper.GetClassifier());
    uint64_t txPackets = 0;
    uint64_t rxPackets = 0;
    uint64_t rxBytes = 0;
    uint64_t lostPackets = 0;
    double delaySeconds = 0.0;
    double jitterSeconds = 0.0;
    for (const auto& [flowId, stats] : monitor->GetFlowStats()) {
        const auto tuple = classifier->FindFlow(flowId);
        if (tuple.destinationPort != port) continue;
        txPackets += stats.txPackets;
        rxPackets += stats.rxPackets;
        rxBytes += stats.rxBytes;
        lostPackets += stats.lostPackets;
        delaySeconds += stats.delaySum.GetSeconds();
        jitterSeconds += stats.jitterSum.GetSeconds();
    }

    const double activeSeconds = duration - 2.5;
    const double pdr = txPackets ? 100.0 * static_cast<double>(rxPackets) / txPackets : 0.0;
    const double throughput = activeSeconds > 0.0
        ? static_cast<double>(rxBytes) * 8.0 / activeSeconds / 1e6
        : 0.0;
    const double delayMs = rxPackets ? 1000.0 * delaySeconds / rxPackets : 0.0;
    const double jitterMs = rxPackets > 1U ? 1000.0 * jitterSeconds / (rxPackets - 1U) : 0.0;

    bool writeHeader = false;
    {
        std::ifstream existing(output, std::ios::binary | std::ios::ate);
        writeHeader = !existing || existing.tellg() == 0;
    }
    std::ofstream out(output, std::ios::app);
    if (writeHeader) {
        out << "protocol,nodes,duration_s,max_speed_mps,seed,pdr_percent,throughput_mbps,"
               "mean_delay_ms,mean_jitter_ms,route_updates,tx_packets,rx_packets,lost_packets\n";
    }
    out << std::fixed << std::setprecision(6)
        << protocol << ',' << nodeCount << ',' << duration << ',' << maxSpeed << ',' << seed << ','
        << pdr << ',' << throughput << ',' << delayMs << ',' << jitterMs << ','
        << controller.updates << ',' << txPackets << ',' << rxPackets << ',' << lostPackets << '\n';

    std::cout << "protocol=" << protocol << " pdr=" << pdr
              << "% throughput=" << throughput << "Mbps delay=" << delayMs << "ms\n";
    Simulator::Destroy();
    return 0;
}
