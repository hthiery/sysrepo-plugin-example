
## compile the plugin

## start the plugin

start the plugin as executable:

./sysrepo-plugin-interfaces -v debug

## importing data

cd examples
cat bridge_config.json | sysrepocfg  -d running -f json -I -t 30 -v3
cat intf_config.json | sysrepocfg  -d running -f json -I -t 30 -v
