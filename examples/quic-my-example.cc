/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2017 CCSL IME-USP
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation;
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * Author: Diego Araújo <diegoamc@ime.usp.br>
 * Heavily based on tcp-bulk-send example.
 *
 * Network topology
 *
 *       n0 ----------- n1
 *            20 Mbps
 *           150 ms
 *
 * - QuicClient (n0) connects on QuicServer (n1)
 * - QuicServer sends data to QuicClient
 * - Tracing of queues and packet receptions to file "quic-example.tr"
 *   and pcap tracing available when tracing is turned on.
 */

#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/quic-utils.h"
#include "ns3/flow-monitor.h"
#include "ns3/flow-monitor-helper.h"
#include "ns3/error-model.h"

using namespace ns3;


NS_LOG_COMPONENT_DEFINE ("QuicModuleExample");

Time last_time(Ptr<FlowMonitor> fm) {
  Time t = Time::Min();
  for(auto e : fm->GetFlowStats())
    t = std::max(t, e.second.timeLastRxPacket);
  return t;
}


#include <string>
using std::string;
int
main (int argc, char *argv[])
{
  Time::SetResolution(Time::NS);

  bool tracing = false;
  uint64_t maxBytes = 10;
  double err = 0;

  string dataRate = "20Mbps";
  string delay = "30ms";

  CommandLine cmd;
  cmd.AddValue ("tracing", "Flag to enable/disable tracing", tracing);
  cmd.AddValue ("maxBytes",
                "Total number of bytes to be sent by the server", maxBytes);
  cmd.AddValue("dataRate", " ", dataRate);
  cmd.AddValue("delay", " ", delay);
  cmd.AddValue("err", " ", err);

  cmd.Parse (argc,argv);

  NS_LOG_INFO ("Creating Topology");

  NodeContainer nodes;
  nodes.Create (2);

  PointToPointHelper pointToPoint;
  pointToPoint.SetDeviceAttribute ("DataRate", StringValue (dataRate));
  pointToPoint.SetChannelAttribute ("Delay", StringValue (delay));

  NetDeviceContainer devices;
  devices = pointToPoint.Install (nodes);

  Ptr<RateErrorModel> em = CreateObject<RateErrorModel> ();
  em->SetAttribute ("ErrorRate", DoubleValue (err));
  //em->SetAttribute ("ErrorUnit", StringValue("ERROR_UNIT_PACKET"));
  devices.Get (0)->SetAttribute ("ReceiveErrorModel", PointerValue (em));
  //devices.Get (1)->SetAttribute ("ReceiveErrorModel", PointerValue (em));

  // Ptr<RateErrorModel> em = CreateObject<RateErrorModel> ();
  // em->SetAttribute ("ErrorRate", DoubleValue (0.00001));
  // devices.Get (1)->SetAttribute ("ReceiveErrorModel", PointerValue (em));

  InternetStackHelper stack;
  stack.Install (nodes);

  Ipv4AddressHelper address;
  address.SetBase ("10.1.1.0", "255.255.255.0");

  Ipv4InterfaceContainer interfaces = address.Assign (devices);

  /* Client */
  uint16_t port = 6121;
  QuicClientHelper clientHelper ("ns3::UdpSocketFactory",
                         InetSocketAddress (interfaces.GetAddress (1), port), true, maxBytes);
  ApplicationContainer clientApps = clientHelper.Install (nodes.Get (0));
  clientApps.Start (Seconds (0.0));

  /* Server */
  QuicServerHelper serverHelper ("ns3::UdpSocketFactory",
                         InetSocketAddress (interfaces.GetAddress (1), port),
                         maxBytes);
  ApplicationContainer serverApps = serverHelper.Install (nodes.Get (1));
  serverApps.Start (Seconds (0.0));

  if (tracing)
    {
      AsciiTraceHelper ascii;
      pointToPoint.EnableAsciiAll (ascii.CreateFileStream ("quic-example.tr"));
      pointToPoint.EnablePcapAll ("quic-example", false);
    }


  // Flow monitor
  Ptr<FlowMonitor> flowMonitor;
  FlowMonitorHelper flowHelper;
  flowMonitor = flowHelper.InstallAll();

  Simulator::Stop(Seconds(2000));
  Simulator::Run ();

  std::cout << "Now " << last_time(flowMonitor).As(Time::S) << std::endl;
  flowMonitor->SerializeToXmlFile("data/quic_simple_" + std::to_string(maxBytes) + "_" + dataRate + "_" + delay + "_" + std::to_string(err) + ".xml", false, false);

  Simulator::Destroy ();
  //Ptr<QuicClient> client = DynamicCast<QuicClient> (clientApps.Get (0));
  //std::cout << "Total Bytes Received: " << client->GetTotalRx () << std::endl;
  return 0;
}


