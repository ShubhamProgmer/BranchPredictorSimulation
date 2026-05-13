#include <iostream>
#include <vector>
#include <cstdlib>
#include <ctime>
using namespace std;

void merge(vector<int>& arr, int l, int m, int r) {
    vector<int> temp;
    int i = l, j = m;

    while (i < m && j < r) {
        if (arr[i] <= arr[j])
            temp.push_back(arr[i++]);
        else
            temp.push_back(arr[j++]);
    }

    while (i < m) temp.push_back(arr[i++]);
    while (j < r) temp.push_back(arr[j++]);

    for (int k = 0; k < temp.size(); k++)
        arr[l + k] = temp[k];
}

void iterativeMergeSort(vector<int>& arr) {
    int n = arr.size();

    for (int currSize = 1; currSize < n; currSize *= 2) {
        for (int leftStart = 0; leftStart < n - 1; leftStart += 2 * currSize) {

            int mid = min(leftStart + currSize, n);
            int rightEnd = min(leftStart + 2 * currSize, n);

            merge(arr, leftStart, mid, rightEnd);
        }
    }
}

int main() {
    srand(time(0));

    vector<int> arr(100);

    // Generate 100 random elements
    for (int i = 0; i < 100; i++)
        arr[i] = rand() % 100;

    iterativeMergeSort(arr);

    // Print sorted array
    for (int x : arr)
        cout << x << " ";

    return 0;
}