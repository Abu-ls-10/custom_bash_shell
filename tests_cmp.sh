#!/bin/bash

# Set the paths to your shell program and the reference shell program
TSH=./tsh
TSHREF=./tshref

# Set the arguments for the shell programs (if any)
TSHARGS="-p"

# Create a file to store the differences
DIFF_FILE=differences.txt
rm -f $DIFF_FILE

# Loop through the test cases (assuming they are named test01, test02, ..., test17)
for i in {1..17}; do
    # Run the test case using your shell program and save the output to a file
    $TSH $TSHARGS < trace$(printf "%02d" $i).txt > your_output_$i.txt

    # Run the same test case using the reference shell program and save the output to a file
    $TSHREF $TSHARGS < trace$(printf "%02d" $i).txt > reference_output_$i.txt

    # Compare the output files using the 'diff' command and append differences to the differences file
    diff -u your_output_$i.txt reference_output_$i.txt >> $DIFF_FILE

    # Check if there are any differences
    if [ $? -eq 0 ]; then
        echo "Test $i passed!"
    else
        echo "Test $i failed. See $DIFF_FILE for details."
    fi
done

# Check if there were any differences overall
if [ -s $DIFF_FILE ]; then
    echo "There were differences found. See $DIFF_FILE for details."
else
    echo "All tests passed!"
    rm -f $DIFF_FILE
fi
