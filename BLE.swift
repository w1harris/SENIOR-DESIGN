//
//  BLE.swift
//  SeniorDesignApp
//
//  Created by Davd Clinkscales on 1/30/24.
//

import SwiftUI
import CoreBluetooth

class BluetoothViewMode : NSObject, ObservableObject {
    private var centralManager:  CBCentralManager?
    private var peripherals: [CBPeripheral] = []
    @Published var peripheralNames: [String] = []
    @Published var isConnectionSuccesful: Bool = false
    @Published var navigateTohomepage: Bool = false
    
    override init(){
        super.init()
        self.centralManager = CBCentralManager(delegate: self, queue: .main)
    }
    
    func connect(to peripheral: CBPeripheral){
        centralManager?.connect(peripheral, options: nil)
    }
    
    func getPeripherals() -> [CBPeripheral] {
        return peripherals
    }
}

extension BluetoothViewMode: CBCentralManagerDelegate {
    func centralManagerDidUpdateState(_ central: CBCentralManager) {
        if central.state == .poweredOn{
            self.centralManager?.scanForPeripherals(withServices: nil)
        }
    }
    func centralManager(_ central: CBCentralManager, didDiscover peripheral: CBPeripheral, advertisementData: [String : Any], rssi RSSI: NSNumber){
        if !peripherals.contains(peripheral) {
            self.peripherals.append(peripheral)
            self.peripheralNames.append(peripheral.name ?? "unknown name")
        }
    }
    func centralManager(_ central: CBCentralManager, didConnect peripheral: CBPeripheral) {
        isConnectionSuccesful = true
    }
    func centralManager(_ central: CBCentralManager, didFailToConnect peripheral: CBPeripheral, error: Error?){
        isConnectionSuccesful = false
    }
    
}

struct BLE: View {
    @Environment(\.presentationMode) var presenttionMode: Binding<PresentationMode>
    @ObservedObject private var bluetoothViewModel = BluetoothViewMode()
    @State private var selectedPeripheral: CBPeripheral?
    
    var body: some View {
        NavigationStack{
            List(bluetoothViewModel.peripheralNames, id:\.self) {
                peripheral in Text(peripheral)
                    .onTapGesture {
                        if let selectedPeripheral = bluetoothViewModel.getPeripherals().first(where: { $0.name == peripheral}){
                            bluetoothViewModel.connect(to: selectedPeripheral)
                        }
                    }
                    .alert(isPresented: $bluetoothViewModel.isConnectionSuccesful){
                        Alert(
                            title: Text("Connected"), message: Text("BLE device is connected!"),dismissButton: .default(
                        Text("Back to Home"),
                        action: {
                            bluetoothViewModel.navigateTohomepage = true
                        }
                        )
                    )
                    }.background().navigationDestination(isPresented: $bluetoothViewModel.navigateTohomepage) {
                        homepage()
                    }
            }
            .navigationTitle("Peripherals")
        }
    }
}

struct BLE_Preiviews: PreviewProvider{
    static var previews: some View {
        BLE()
    }

}
