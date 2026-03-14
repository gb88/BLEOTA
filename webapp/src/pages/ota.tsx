import React, { useState } from "react";
import {
  IonContent,
  IonHeader,
  IonPage,
  IonTitle,
  IonFooter,
  IonToolbar,
  IonText,
  IonButton,
  IonThumbnail,
  IonCardSubtitle,
  IonCardTitle,
  IonCardHeader,
  IonCardContent,
  IonCard,
  useIonToast,
  IonGrid,
  IonRow,
  IonCol,
  IonImg,
  IonSegment,
  IonSegmentButton,
  IonLabel,
} from "@ionic/react";

import {
  BleClient,
  numbersToDataView,
  numberToUUID,
} from "@capacitor-community/bluetooth-le";
import { Buffer } from "buffer";
import { bstr, buf, str } from "crc-32";
import ProgressBar from "@ramonak/react-progress-bar";
import "./ota.css";
var version = "Version 0.03";
var CRC32 = require("crc-32");
var ACK = 1;
var clientDevice: any;
var cmdStatus = 0;
var fwStatus = 0;
var expected_index = 0;
const Ota: React.FC = () => {
  const [device, setDevice] = useState<string>("");
  const [Model, setModel] = useState<string>("");
  const [SerialNumber, setSerialNumber] = useState<string>("");
  const [Manufacturer, setManufacturer] = useState<string>("");
  const [HWVersion, setHWVersion] = useState<string>("");
  const [SWVersion, setSWVersion] = useState<string>("");
  const [DISInfo, setDis] = useState(false);
  const [isConnected, setConnection] = useState(false);
  const [isFileLoaded, setLoadFile] = useState(false);
  const [showBar, setProgressBar] = useState(false);
  const [otaType, setotaType] = useState<string>("");
  const [otaFile, setOtaFile] = useState<ArrayBuffer>();
  const [progress, setProgress] = useState(0);
  const [upload_speed, setUploadSpeed] = useState(0.00);
  const [barStatus, setBarStatus] = useState<string>("danger");
  const [showStatus, setShowStatus] = useState(false);
  const [status, setStatus] = useState<string>("");
  // const [cmdStatus, setcmdStatus] = useState(0);
  //const [fwStatus, setfwStatus] = useState(0);
  const [present] = useIonToast();
  const onDisconnect = (deviceId: string) => {
    console.log(`device ${deviceId} disconnected`);
    setConnection(false);
    setLoadFile(false);
    setProgressBar(false);
  };

  function crc16(init: number, data: Uint8Array, len: number): number {
    let crc = init;

    for (let i = 0; i < len; i++) {
      crc ^= data[i] << 8;

      for (let j = 0; j < 8; j++) {
        if (crc & 0x8000) {
          crc = ((crc << 1) ^ 0x1021) & 0xffff;
        } else {
          crc <<= 1;
        }
      }
    }

    return crc & 0xffff;
  }
  const ab2str = (buf: ArrayBuffer) => {
    return new TextDecoder().decode(buf);
  };

  const presentToast = (
    position: "top" | "middle" | "bottom",
    message: string
  ) => {
    present({
      message: message,
      duration: 1500,
      position: position,
    });
  };

  async function send_firmware(
    packet_size: number,
    file: ArrayBuffer,
    deviceId: any
  ) {
    //self.start_time = time.time()
	const startTime = new Date().getTime();
    let index = 0;
    let written_size = 0;
    while (written_size < file.byteLength) {
      let sector_size = 0;
      let sequence = 0;
      let crc = 0;
      //let to_read = 0;
      let f_last = false;
      let sector = file.slice(written_size, written_size + 4096);
      if (sector.byteLength === 0) {
        break;
      }
      while (sector_size < sector.byteLength) {
        let to_read = packet_size - 3;
        if (sector_size + to_read > sector.byteLength) {
          to_read = sector.byteLength - sector_size;
		  f_last = true;
        }
        let sector_data = sector.slice(sector_size, sector_size + to_read);
        sector_size = sector_size + to_read;
        if (sector_size >= 4096) f_last = true;
        crc = crc16(
          crc,
          new Uint8Array(sector_data, 0, sector_data.byteLength),
          sector_data.byteLength
        );
        if (f_last) sequence = 0xff;
        let packet = Buffer.alloc(to_read + 3);
        packet[0] = index & 0xff;
        packet[1] = (index >> 8) & 0xff;
        packet[2] = sequence;
        Buffer.from(sector_data).copy(packet, 3);
        written_size = written_size + sector_data.byteLength;
		//console.log(written_size);

        if (f_last) {
	      let p_status = Math.round((100 * written_size) / file.byteLength);
		  setProgress(p_status);
		  setUploadSpeed(written_size *1000 / 1024 / (new Date().getTime() - startTime));
          let crc_data = Buffer.alloc(2);
          crc_data[0] = crc & 0xff;
          crc_data[1] = (crc >> 8) & 0xff;
          packet = Buffer.concat([packet, crc_data]);
        }
        //console.log("Send sector %d sequence %d sector end index %d", index, sequence, packet.length)
        //setfwStatus(0);
        fwStatus = 0;
        expected_index = index;
		//console.log("Send " + index);
        await BleClient.writeWithoutResponse(
          deviceId,
          "00008018-0000-1000-8000-00805f9b34fb",
          "00008020-0000-1000-8000-00805f9b34fb",
          numbersToDataView(Array.prototype.slice.call(packet))
        );
        if (f_last && fwStatus == 0) {
          const fw_ack = await waitForAnsFw(5000);
          if (!fw_ack) {
			setBarStatus("danger");
			setShowStatus(true); 
            console.log("FW NACK");
            return;
          }
        }
        sequence = sequence + 1;
      }
      index = index + 1;
    }
  }

  //function parseFirmwareNotification(value: DataView): number {
  function parseFirmwareNotification(value: DataView): void {
    //console.log(value);
    if (value.buffer.byteLength >= 20) {
      let crc = value.getUint16(18, true);
      let crc_recv = crc16(0, new Uint8Array(value.buffer, 0, 16), 18);
      if (crc_recv == crc) {
        let fw_ans = value.getUint16(2, true);
        if (fw_ans == 0 && expected_index == value.getUint16(0, true)) {
          fwStatus = 1;
		 // console.log("OK");
        } else if (fw_ans == 1 && expected_index == value.getUint16(0, true)) {
          fwStatus = 0;
		  setStatus("CRC Error");
		  console.log("CRC Error");
          //TODO: crc error
        } else if (fw_ans == 2) {
          //&& (expected_index == value.getUint16(0,true))???
          fwStatus = 0;
		  setStatus("Index Error");
		  console.log("Index Error");
          //TODO: sector index
          // bytes(4 ~ 5) indicates the desired Sector_Index
        } else if (fw_ans == 3 && expected_index == value.getUint16(0, true)) {
          fwStatus = 0;
		  setStatus("Payload length Error");
		  console.log("Payload length Error");
          //TODO: Payload length error
        }
        //setfwStatus(1);
      }
    }
  }
  function parseCommandNotification(value: DataView): void {
    if (value.buffer.byteLength >= 20) {
      let crc = value.getUint16(18, true);
      let crc_recv = crc16(0, new Uint8Array(value.buffer, 0, 16), 18);
      if (crc_recv == crc) {
        if (value.getUint16(0, true) == 3) {
          if (
            value.getUint16(2, true) == 1 ||
            value.getUint16(2, true) == 2 ||
            value.getUint16(2, true) == 4
          ) {
            let ans = value.getUint16(4, true);
            if (ans == 0) {
              //ok
              //setcmdStatus(1);
              cmdStatus = 1;
              console.log(value);
            } else if (ans == 1) {
			  if(value.getUint16(2, true) == 1)
			  	setStatus("NACK on START command");
		      else if(value.getUint16(2, true) == 1)
			  	setStatus("NACK on STOP command");
              //not ok
              cmdStatus = 0;
              //setcmdStatus(0);
            } else if (ans == 3) {
			  setStatus("Signature Error");
              //signature error TODO
              //setcmdStatus(0);
              cmdStatus = 0;
            }
          }
        }
      }
    }
  }

  const ScanClick = async () => {
    console.log("Scan Clicked");
    if (isConnected) {
		setConnection(false);
		BleClient.disconnect(clientDevice.deviceId);
	}
    try {
	  setShowStatus(false);
      setConnection(false);
      setLoadFile(false);
      setProgressBar(false);
      setotaType("undefined");
      setDis(false);
      await BleClient.initialize();
      clientDevice = await BleClient.requestDevice({
        services: ["00008018-0000-1000-8000-00805f9b34fb"],
        optionalServices: [numberToUUID(0x180a)],
      });
      
      await BleClient.connect(clientDevice.deviceId, (deviceId) =>
        onDisconnect(deviceId)
      );
	  setDevice(clientDevice.name as string);
      console.log("Connected to device");
      await BleClient.startNotifications(
        clientDevice.deviceId,
        "00008018-0000-1000-8000-00805f9b34fb",
        "00008020-0000-1000-8000-00805f9b34fb",
        (value) => {
          parseFirmwareNotification(value);
        }
      );
      await BleClient.startNotifications(
        clientDevice.deviceId,
        "00008018-0000-1000-8000-00805f9b34fb",
        "00008022-0000-1000-8000-00805f9b34fb",
        (value) => {
          parseCommandNotification(value);
        }
      );

      try {
        const response = await BleClient.read(
          clientDevice.deviceId,
          numberToUUID(0x180a),
          numberToUUID(0x2a24),
          { timeout: 500 }
        );
        setModel(ab2str(response.buffer));
        setDis(true);
      } catch (error) {
        console.log("timeout 0x2a24");
        setModel("");
      }
      try {
        const response = await BleClient.read(
          clientDevice.deviceId,
          numberToUUID(0x180a),
          numberToUUID(0x2a25),
          { timeout: 500 }
        );
        setSerialNumber(ab2str(response.buffer));
        setDis(true);
      } catch (error) {
        console.log("timeout 0x2a25");
        setSerialNumber("");
      }
      try {
        const response = await BleClient.read(
          clientDevice.deviceId,
          numberToUUID(0x180a),
          numberToUUID(0x2a26),
          { timeout: 500 }
        );
        setSWVersion(ab2str(response.buffer));
        setDis(true);
      } catch (error) {
        console.log("timeout 0x2a26");
        setSWVersion("");
      }
      try {
        const response = await BleClient.read(
          clientDevice.deviceId,
          numberToUUID(0x180a),
          numberToUUID(0x2a27),
          { timeout: 500 }
        );
        setHWVersion(ab2str(response.buffer));
        setDis(true);
      } catch (error) {
        console.log("timeout 0x2a27");
        setHWVersion("");
      }
      try {
        const response = await BleClient.read(
          clientDevice.deviceId,
          numberToUUID(0x180a),
          numberToUUID(0x2a29),
          { timeout: 500 }
        );
        setManufacturer(ab2str(response.buffer));
        setDis(true);
      } catch (error) {
        console.log("timeout 0x2a29");
        setManufacturer("");
      }
      //presentToast('middle', "Connected");
      setConnection(true);
    } catch (e) {
      console.error(e);
    }
  };
  const DisconnectClick = () => {
    setLoadFile(false);
    setProgressBar(false);
    setotaType("undefined");
    if (isConnected) {
      setConnection(false);
      BleClient.disconnect(clientDevice.deviceId);
    }
  };
  // Function that returns a Promise and resolves when the condition is settled
  function waitForAnsCommand(timeout: number): Promise<boolean> {
    return new Promise<boolean>((resolve) => {
      if (cmdStatus == 1) resolve(true);
      let intervalId: NodeJS.Timeout | null = null;

      const checkCondition = () => {
        // Replace the condition below with your actual condition
        if (cmdStatus === ACK) {
        //  console.log("ack");
          if (intervalId !== null) {
            clearInterval(intervalId);
            intervalId = null;
          }
          resolve(true);
        }
      };

      intervalId = setInterval(checkCondition, 50);

      // Timeout to reject the Promise if the condition is not settled within the specified time
      setTimeout(() => {
        if (intervalId !== null) {
          clearInterval(intervalId);
          console.log("timeout1");
          resolve(false);
        }
      }, timeout);
    });
  }

  function waitForAnsFw(timeout: number): Promise<boolean> {
    return new Promise<boolean>((resolve) => {
      if (fwStatus == 1) {
        resolve(true);
      }
      let intervalId: NodeJS.Timeout | null = null;

      const checkCondition = () => {
        // Replace the condition below with your actual condition
        if (fwStatus == 1) {
          if (intervalId !== null) {
            clearInterval(intervalId);
            intervalId = null;
          }
          resolve(true);
        }
      };

      intervalId = setInterval(checkCondition, 50);

      // Timeout to reject the Promise if the condition is not settled within the specified time
      setTimeout(() => {
        if (intervalId !== null) {
          clearInterval(intervalId);
          console.log("timeout2");
          resolve(false);
        }
      }, timeout);
    });
  }
  const OtaClick = async () => {
    if (otaFile == null) return;
    setLoadFile(false);
    let buffer = Buffer.alloc(20);
    if (otaType == "app") {
      console.log("Start App OTA");
      buffer[0] = 0x01;
      buffer[1] = 0x00;
    } else if (otaType == "spiffs") {
      console.log("Start SPIFFS OTA");
      buffer[0] = 0x04;
      buffer[1] = 0x00;
    }
    buffer.writeUInt32LE(otaFile.byteLength, 2);
    let crc = crc16(0, buffer, 18);
    buffer.writeUInt16LE(crc, 18);
    console.log(buffer);
    //setcmdStatus(0);
    cmdStatus = 0;
    await BleClient.write(
      clientDevice.deviceId,
      "00008018-0000-1000-8000-00805f9b34fb",
      "00008022-0000-1000-8000-00805f9b34fb",
      numbersToDataView(Array.prototype.slice.call(buffer))
    );
    let cmd_ack = await waitForAnsCommand(5000);
    if (!cmd_ack) {
      //TODO: nack error manage it
	  setBarStatus("danger");
	  setShowStatus(true); 
      console.log("NACK");
      return;
    }
    console.log("cmd acked");
    setProgress(0);
    setProgressBar(true);
    await send_firmware(510, otaFile, clientDevice.deviceId);
	console.log("Stop OTA");
	buffer.fill(0);
	buffer[0] = 0x02;
	buffer[1] = 0x00;
	crc = crc16(0, buffer, 18);
	buffer.writeUInt16LE(crc, 18);
	console.log(buffer);
	//setcmdStatus(0);
	cmdStatus = 0;
	await BleClient.write(
	clientDevice.deviceId,
	"00008018-0000-1000-8000-00805f9b34fb",
	"00008022-0000-1000-8000-00805f9b34fb",
	numbersToDataView(Array.prototype.slice.call(buffer))
	);
	cmd_ack = await waitForAnsCommand(5000);
	if (!cmd_ack) {
	//TODO: nack error manage it
	setBarStatus("danger");
	setShowStatus(true); 
	console.log("NACK");
	return;
	}
    setotaType("undefined");
    setConnection(false);
    setProgressBar(false);
    setLoadFile(false);
	setStatus("OTA Done");
	setBarStatus("success");
	setShowStatus(true);
    BleClient.disconnect(clientDevice.deviceId);
  };
  const onFileChange = (event: React.ChangeEvent<HTMLInputElement>) => {
    if (!event.target.files) return;
    let sourceFile = event.target.files[0];
    const reader = new FileReader();
    reader.addEventListener("load", (event) => {
      setOtaFile(reader.result as ArrayBuffer);
      setLoadFile(true);
      console.log(reader.result);
    });
    reader.readAsArrayBuffer(sourceFile);
  };
  const SelectionChange = (event: CustomEvent) => {
    // Access the selected segment value using event.detail.value
    setotaType(event.detail.value);
    console.log(event.detail.value);
  };

  return (
    <IonPage>
      <IonHeader>
        <IonToolbar>
          <IonTitle>
            {!isConnected && (<IonButton
              onClick={() => {
                ScanClick();
              }}
            >
              Scan
            </IonButton>)}
			{isConnected && (<IonButton
			class="ion-float-right"
			onClick={() => {
				DisconnectClick();
			}}
			color="danger"
			>
			Disconnect
			</IonButton>)}
          </IonTitle>
        </IonToolbar>
      </IonHeader>
	  {showStatus && (<IonCard color={barStatus}>  
				<IonCardContent>
                <IonGrid>
                  <IonRow className="ion-justify-content-center ion-align-items-center">
				  <IonText>{status}</IonText>
                  </IonRow>
                </IonGrid>
				</IonCardContent></IonCard>
			)}
      {isConnected && (
        <IonContent class="ion-padding">
          <IonHeader collapse="condense">
            <IonToolbar>
              <IonTitle size="large">Tab 1</IonTitle>
            </IonToolbar>
          </IonHeader>
          <h1>{device}</h1>
              {DISInfo && (
				<IonCard>  
				<IonCardHeader>
					<IonCardTitle>Device Information</IonCardTitle>
				</IonCardHeader>
				<IonCardContent>
                <IonGrid>
                  <IonRow>
                    <IonCol>
                      <IonCardSubtitle>
                        Manufacturer: {Manufacturer}
                      </IonCardSubtitle>
                      <IonCardSubtitle>Model: {Model}</IonCardSubtitle>
                      <IonCardSubtitle>HW Version: {HWVersion}</IonCardSubtitle>
                      <IonCardSubtitle>FW Version: {SWVersion}</IonCardSubtitle>
                    </IonCol>
                  </IonRow>
                </IonGrid>
				</IonCardContent></IonCard>
              )}
			  <IonCard>  
				<IonCardContent>
				{!showBar && (<IonGrid>
                <IonRow>
                  <IonSegment value={otaType} onIonChange={SelectionChange}>
                    <IonSegmentButton value="app">
                      <IonLabel>App</IonLabel>
                    </IonSegmentButton>
                    <IonSegmentButton value="spiffs">
                      <IonLabel>SPIFFS</IonLabel>
                    </IonSegmentButton>
                  </IonSegment>
                </IonRow>
              </IonGrid>)}
			  {otaType != "undefined" && (
                <IonGrid>
                  <IonRow className="ion-justify-content-center ion-align-items-center">
                    <IonCol size="12" sizeMd="6" sizeLg="4">
                      {!showBar && (
                        <input type="file" onChange={onFileChange} />
                      )}
                    </IonCol>
                  </IonRow>
				  <IonRow className="ion-justify-content-center ion-align-items-center">
				  {showBar && (<IonLabel>{upload_speed.toFixed(2) + " kB/s"}</IonLabel>)}
				  </IonRow>
                  <IonRow className="ion-justify-content-center ion-align-items-center">
                    <IonCol>
                      {showBar && (
                        <ProgressBar
                          completed={progress}
                          labelAlignment="center"
                          bgColor="#3880ff"
                        />
                      )}
					</IonCol>
				</IonRow>
				<IonRow>
                    <IonCol>
						{isFileLoaded && (
                        <IonButton
                          onClick={() => {
                            OtaClick();
                          }}
                        >
                          Start
                        </IonButton>
                      	)}
                    </IonCol>
                  </IonRow>
                </IonGrid>
              )}
			</IonCardContent></IonCard>
			
        </IonContent>
      )}
	  
      <IonFooter>
        <IonToolbar>
          <IonTitle>{version}</IonTitle>

        </IonToolbar>
      </IonFooter>
    </IonPage>
  );
};

export default Ota;
