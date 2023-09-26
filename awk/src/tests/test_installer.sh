install_tests(){
  t_names=$1
  t_data=$2
  t_mode=$3
  assets_dir=$(pwd)/assets
  count=0
  for test in $t_names
  do
      echo "$test"
      test_output="$(/bin/sh awk_guider.sh $test $mode $assets_dir)" || exit 1
      /bin/sh awk_runner.sh $test $test_data $test_output
      count=$((count+1))
  done

  echo "Install $count test results"
}