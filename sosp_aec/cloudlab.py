""" AEC """

# Import the Portal object.
import geni.portal as portal
# Import the ProtoGENI library.
import geni.rspec.pg as pg
# Import the Emulab specific extensions.
import geni.rspec.emulab as emulab

# Create a portal object,
pc = portal.Context()

# Create a Request object to start building the RSpec.
request = pc.makeRequestRSpec()

# Node server
node_server = request.RawPC('server')
node_server.routable_control_ip = True
node_server.hardware_type = 'c6420'
# node_server.disk_image = 'urn:publicid:IDN+emulab.net+image+emulab-ops//UBUNTU16-64-STD'
node_server.disk_image = 'urn:publicid:IDN+clemson.cloudlab.us+image+psp-PG0:sosp_aec.server:3'
iface1 = node_server.addInterface()
iface1.addAddress(pg.IPv4Address("192.168.10.10", "255.255.255.0"))
lan = request.LAN("lan")
lan.addInterface(iface1)

# Node client
ifaces = []
for i in range(6):
    node_client = request.RawPC('client_'+str(i))
    node_client.routable_control_ip = True
    node_client.hardware_type = 'c6420'
    # node_client.disk_image = 'urn:publicid:IDN+emulab.net+image+emulab-ops//UBUNTU16-64-STD'
    node_client.disk_image = 'urn:publicid:IDN+clemson.cloudlab.us+image+psp-PG0:sosp_aec.client'
    clt_iface = node_client.addInterface()
    clt_iface.addAddress(pg.IPv4Address("192.168.10.1"+str(i+1), "255.255.255.0"))
    ifaces.append(clt_iface)

for iface in ifaces:
    lan.addInterface(iface)

pc.defineParameter("sameSwitch",  "No Interswitch Links", portal.ParameterType.BOOLEAN, True)
params = pc.bindParameters()
pc.verifyParameters()

# Print the generated rspec
pc.printRequestRSpec(request)
