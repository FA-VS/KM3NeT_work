#! /bin/bash
set -u

runlist_file=$1
proc_ver=$2 # e.g. "v9.0"

cut="muonscore<0.1 && noisescore < 0.5 && runs_neutrino2024_veto_sparks && (Evt->trks[0].dir.z>0 || Evt->trks[1].dir.z>0)"
selector_dir="/project/antares/fvazquez/irf/rootfiles/Selector"
local_data_dir="/data/antares/users/hlumengo/Merged_dsts"
#pid_dir="/data/antares/users/hlumengo/PID/${proc_ver}_selection_trees"
#treename="sel"

# path in storage
declare -A type_path_arr=( ["data"]="data" ["mu"]="mc/atm_muon" ["nu"]="mc/atm_neutrino" ["noise"]="mc/pure_noise")
# snakemake file naming convention
declare -A type_nameconv_arr=( ["data"]="data" ["mu"]="mc.mupage"   ["nu"]="mc.gsg_neutrinos" ["noise"]="mc.pure_noise")

download_method="rucio" # rucio or xrootd or irods (in that order of preference?)
# TODO: Use file streaming instead of download if using xrootd (and, eventually, rucio)

if [ $download_method == "rucio" ]; then
  # If using Rucio, request replication to disk first
  rucio_replication_scope="grid_datasets"
  rucio_dataset="Replicated_ForSelector"
  rucio did add --type dataset -d ${rucio_replication_scope}:${rucio_dataset}
  while IFS= read -r line; do
    # split detid, run from $line
    line_arr=( $line )
    detid=${line_arr[0]}
    run=${line_arr[1]}

    # Add to replication dataset
    rucio_scope="processed_data"
    filename="KM3NeT_${detid}_${run}.${snakemake_fileext}.${proc_ver}.root"
    rucio did content add --to ${rucio_replication_scope}:${rucio_dataset} -d ${rucio_scope}:${filename}
  done < $runlist_file
  rucio rule add -d ${rucio_replication_scope}:${rucio_dataset} --copies 1 --rses type=DISK --lifetime 86400
fi

DownloadFile () {
  local detid=$1
  local run=$2
  local type=$3 #data or mu or nu or noise
  local type_path=type_path_arr[$type] #data or mc/atm_muon or mc/atm_neutrino or mc/pure_noise
  local type_nameconv=type_nameconv_arr[$type] #data or mc.mupage or mc.gsg_neutrinos or mc.pure_noise

  filename_regex="KM3NeT_${detid}_${run}\.${type_nameconv}.*\.${proc_ver}\.root"
  local_data_dir_temp="${local_data_dir}/${type_path}/KM3NeT_${detid}/${proc_ver}/dst"
  mkdir -p $local_data_dir_temp

  # Only try one of the following / whichever one works

  if [ $download_method == "xrootd" ]; then
    #xrootd (only from CC-IN2P3!!)
    xrootd_host="root://ccxroot:1999"
    xrootd_dir="/hpss/in2p3.fr/group/km3net/${type_path}/KM3NeT_${detid}/${proc_ver}/dst"
    filename=$(xrdfs $xrootd_host ls $xrootd_dir | grep -oE "$filename_regex")
    xrdcp "${xrootd_host}/${xrootd_dir}/${filename}" "${local_data_dir_temp}/${filename}"
  fi

  if [ $download_method == "rucio" ]; then
    #rucio
    rucio_scope="processed_data"
    filename=$(rucio did list -d ${rucio_scope}: --filter type=FILE | grep -oE "$filename_regex")
    rucio download --ignore-checksum -d ${rucio_scope}:${filename} --dir $local_data_dir_temp
  fi

  if [ $download_method == "irods" ]; then
    #irods
    #irods_dir="/hpss/in2p3/km3net/"
    irods_dir="/in2p3/km3net/${type_path}/KM3NeT_${detid}/${proc_ver}/dst"
    filename=$(ils $irods_dir | grep -oE "$filename_regex")
    iget -N 0 "${irods_dir}/${filename}" "${local_data_dir_temp}/${filename}"
  fi

}

CallSelector () {
  local detid=$1
  local run=$2
  local type=$3 #data or mu or nu or noise
  local type_path=type_path_arr[$type] #data or mc/atm_muon or mc/atm_neutrino or mc/pure_noise
  local type_nameconv=type_nameconv_arr[$type] #data or mc.mupage or mc.gsg_neutrinos or mc.pure_noise

  local_data_dir_temp="${local_data_dir}/${type_path}/KM3NeT_${detid}/${proc_ver}/dst"
  filename=$(ls $local_data_dir_temp | grep -oE "KM3NeT_${detid}_${run}\.${type_nameconv}.*\.${proc_ver}\.root")

  selector_dir_temp="${selector_dir}/${type_path}/KM3NeT_${detid}/${proc_ver}"
  mkdir -p $selector_dir_temp
  outputfile="${selector_dir_temp}/KM3NeT_${detid}_${run}_${type}_${proc_ver}.root"

  # pidfile="$pid_dir/KM3NeT_00000${detid}_${quality}/SelectedEventsTree_PID_${inputname}.root"
  $IRF_DIR/Selector/SelectorFromJSON -o "${selector_dir_temp}/$outputfile" -c "$cut" -i "${local_data_dir_temp}/$filename" # -f "$pidfile,$treename"
}


while IFS= read -r line; do
  # split detid, run from $line
  line_arr=( $line )
  detid=${line_arr[0]}
  run=${line_arr[1]}

  # data
  type="data"
  DownloadFile "$detid" "$run" "$type"
  CallSelector "$detid" "$run" "$type"

  # muons
  type="mu"
  DownloadFile "$detid" "$run" "$type"
  CallSelector "$detid" "$run" "$type"

  # neutrinos
  type="nu"
  DownloadFile "$detid" "$run" "$type"
  CallSelector "$detid" "$run" "$type"

  # noise
  type="noise"
  DownloadFile "$detid" "$run" "$type"
  CallSelector "$detid" "$run" "$type"

done < $runlist_file


