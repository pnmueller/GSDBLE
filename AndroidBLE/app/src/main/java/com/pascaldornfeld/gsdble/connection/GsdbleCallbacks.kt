package com.pascaldornfeld.gsdble.connection

import android.bluetooth.BluetoothDevice
import android.util.Log
import com.pascaldornfeld.gsdble.ReadDeviceIfc
import com.pascaldornfeld.gsdble.WriteDeviceIfc
import no.nordicsemi.android.ble.BleManagerCallbacks

/**
 * callback for bluetooth events happening with gsdble device
 *
 * @param readDeviceIfc interface to notify about connection events
 * @param writeDeviceIfc interface to perform actions (disconnect on error)
 */
class GsdbleCallbacks(
    private val readDeviceIfc: ReadDeviceIfc,
    private val writeDeviceIfc: WriteDeviceIfc
) : BleManagerCallbacks {

    override fun onDeviceDisconnected(device: BluetoothDevice) =
        readDeviceIfc.afterDisconnect()

    // disconnect because of errors

    override fun onDeviceNotSupported(device: BluetoothDevice) {
        Log.w(TAG, "Disconnect because device not supported")
        writeDeviceIfc.doDisconnect()
    }

    override fun onBondingFailed(device: BluetoothDevice) {
        Log.w(TAG, "Disconnect because bonding failed")
        writeDeviceIfc.doDisconnect()
    }

    override fun onBondingRequired(device: BluetoothDevice) {
        Log.w(TAG, "Disconnect because bonding required")
        writeDeviceIfc.doDisconnect()
    }

    override fun onLinkLossOccurred(device: BluetoothDevice) {
        Log.w(TAG, "Disconnect because link loss")
        writeDeviceIfc.doDisconnect()
    }

    override fun onError(device: BluetoothDevice, message: String, errorCode: Int) {
        Log.w(TAG, "Disconnect because error $errorCode")
        writeDeviceIfc.doDisconnect()
    }

    // ignored

    override fun onDeviceReady(device: BluetoothDevice) = Unit
    override fun onDeviceDisconnecting(device: BluetoothDevice) = Unit
    override fun onDeviceConnected(device: BluetoothDevice) = Unit
    override fun onServicesDiscovered(device: BluetoothDevice, optionalServicesFound: Boolean) =
        Unit

    override fun onBonded(device: BluetoothDevice) = Unit
    override fun onDeviceConnecting(device: BluetoothDevice) = Unit

    companion object {
        private val TAG = GsdbleCallbacks::class.java.simpleName.filter { it.isUpperCase() }
    }
}