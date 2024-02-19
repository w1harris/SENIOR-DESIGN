//
//  LockScreen.swift
//  SeniorDesignApp
//
//  Created by Davd Clinkscales on 1/29/24.
//

import SwiftUI

struct LockScreen: View {
    @State var unLocked = false
    var body: some View {
      

        NavigationView{
            
            ZStack{
                //lockscreen
                if unLocked {
                  homepage()
                }
                else {
                    Lock(unLocked: $unLocked)
                }
                
                    
            }.preferredColorScheme(unLocked ? .light : .dark)
            
        }
        
    }
}

struct Lock: View {
    @State var password = ""
    
    @AppStorage("lock_Password") var key = "0919"
    @Binding var unLocked : Bool
    @State var wrongPassword = false
    let height = UIScreen.main.bounds.width
    
    var body: some View{
        VStack {
            HStack{
                Spacer(minLength: /*@START_MENU_TOKEN@*/0/*@END_MENU_TOKEN@*/)
                
                Menu(content:{
                    Label(
                        title:{ Text("Help") },
                        icon: { Image(systemName:"info.circle.fill") }).onTapGesture(perform: {
                            // performs action
                        })
                    Label(
                        title:{ Text("Reset Password") },
                        icon: { Image(systemName:"key.fill") }).onTapGesture(perform: {
                            
                        })
                    
                }) {
                    Image(systemName: "gearshape").renderingMode(.template).resizable().frame(width: 30, height: 30).foregroundColor(.white).padding()
                }
                
            }
            .padding(.leading)
            
            Image(systemName: "person.crop.circle.fill")
                .resizable()
                .frame(width:95, height: 95)
                .padding(.top,20)
            Text("Enter Pin to Unlock")
                .font(.title2)
                .fontWeight(.bold).padding(.top,20)
            HStack(spacing: 22){
                
                //Password Circle View
                ForEach(0..<4,id: \.self){index in
                    
                    PasswordView(index: index, password: $password)
                    
                }
            }.padding(.top, height < 750 ? 20 : 30)
            
            Spacer(minLength: 0)
            
            Text(wrongPassword ? "Incorrect Pin" : "")
                .foregroundColor(.red)
                .fontWeight(/*@START_MENU_TOKEN@*/.bold/*@END_MENU_TOKEN@*/)
            
            Spacer(minLength: /*@START_MENU_TOKEN@*/0/*@END_MENU_TOKEN@*/)
            
            LazyVGrid(columns: Array(repeating: GridItem(.flexible()), count: 3), spacing: height < 750 ? 5:15){
                
                ForEach(1...9,id: \.self){value in
                    PasswordButton(value: "\(value)", password:$password, key: $key, unLocked: $unLocked, wrongPass: $wrongPassword)
                }
                PasswordButton(value: "delete.fill", password: $password, key: $key, unLocked: $unLocked, wrongPass: $wrongPassword)
                PasswordButton(value: "0", password:  $password, key: $key, unLocked: $unLocked, wrongPass: $wrongPassword)
                
            }
            .padding(.bottom)
            Spacer(minLength: /*@START_MENU_TOKEN@*/0/*@END_MENU_TOKEN@*/)
        }
        .navigationTitle("")
        .navigationBarHidden(true)
    }
}


struct LockScreen_Previews: PreviewProvider {
    static var previews: some View {
       LockScreen()
    }
}

struct PasswordView : View {
    var index: Int
    @Binding var password : String
    
    var body: some View{
        
        ZStack{
            Circle()
                .stroke(Color.white, lineWidth: 2)
                .frame(width: 30, height: 30)
            
            if password.count > index {
                Circle()
                    .fill(Color.white)
                    .frame(width: 30, height: 30)
            }
               
        }
    }
    
}

struct PasswordButton : View {
    var value : String
    @Binding var password : String
    @Binding var key : String
    @Binding var unLocked : Bool
    @Binding var wrongPass : Bool
    
    var body: some View {
        Button(action: setPassword, label: {
            
            VStack{
                if value.count > 1 {
                    Image(systemName: "delete.left")
                        .font(.system(size:24))
                        .foregroundColor(.white)
                }
                else {
                    Text(value)
                        .font(.title)
                        .foregroundColor(.white)
                }
            }
            .padding()
            
        })
    }
    func setPassword(){
        withAnimation{
            if value.count > 1 {
                if password.count != 0 {
                    password.removeLast()
                }
            }
            else {
                if password.count != 4 {
                    password.append(value)
                    DispatchQueue.main.asyncAfter(deadline: .now() + 0.3) {
                        
                        withAnimation{
                            if password.count == 4{
                                if password == key{
                                    unLocked = true
                                }
                                else{
                                    wrongPass = true
                                    password.removeAll()
                                }
                            }
                        }
                    }
                }
            }
        }
    }
}

